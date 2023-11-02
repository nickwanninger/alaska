#include <alaska/Passes.h>
#include <alaska/Translations.h>
#include <alaska/Utils.h>
#include <optional>

#include "llvm/Transforms/Utils/EscapeEnumerator.h"
#include "llvm/IR/PassManager.h"
#include "llvm/ADT/SparseBitVector.h"

using namespace llvm;



GetElementPtrInst *CreateGEP(
    LLVMContext &Context, IRBuilder<> &B, Type *Ty, Value *BasePtr, int Idx, const char *Name) {
  Value *Indices[] = {ConstantInt::get(Type::getInt32Ty(Context), 0),
      ConstantInt::get(Type::getInt32Ty(Context), Idx)};
  Value *Val = B.CreateGEP(Ty, BasePtr, Indices, Name);

  assert(isa<GetElementPtrInst>(Val) && "Unexpected folded constant");

  return dyn_cast<GetElementPtrInst>(Val);
}

GetElementPtrInst *CreateGEP(LLVMContext &Context, IRBuilder<> &B, Type *Ty, Value *BasePtr,
    int Idx, int Idx2, const char *Name) {
  Value *Indices[] = {ConstantInt::get(Type::getInt32Ty(Context), 0),
      ConstantInt::get(Type::getInt32Ty(Context), Idx),
      ConstantInt::get(Type::getInt32Ty(Context), Idx2)};
  Value *Val = B.CreateGEP(Ty, BasePtr, Indices, Name);

  assert(isa<GetElementPtrInst>(Val) && "Unexpected folded constant");

  return dyn_cast<GetElementPtrInst>(Val);
}

PreservedAnalyses LockInsertionPass::run(Module &M, ModuleAnalysisManager &AM) {
  if (getenv("ALASKA_NO_TRACKING") != NULL) {
    return PreservedAnalyses::all();
  }



  // TODO:
  // for each function
  //    extract the translations
  //    for each the escape sites (polls and calls)
  //       if a translation is alive at the escape site
  //           add it to the "must track" set
  //    allocate an array on the stack that is big enough to track the "must track" set
  //    for each element in the "must track" set
  //       store to one entry of that stack array right before translation
  //    Replace call instructions with associated statepoint gc intrinsics
  //       ... passing the stack array as an arg in the "deopt" bundle

#if 1

  for (auto &F : M) {
    // Ignore functions with no bodies
    if (F.empty()) continue;

    // If the function is simple, it can't poll, so we can skip it! Yippee!
    if (F.hasFnAttribute("alaska_is_simple")) {
      continue;
    }
    // Extract all the locks from the function
    auto translations = alaska::extractTranslations(F);
    // If the function had no locks, don't do anything
    if (translations.empty()) continue;

    // The set of translations that *must* be translated.
    std::set<alaska::Translation *> mustTrack;
    // A set of all
    std::set<CallInst *> statepointCalls;


    for (auto &BB : F) {
      for (auto &I : BB) {
        if (auto *call = dyn_cast<IntrinsicInst>(&I)) {
          continue;
        }
        if (auto *call = dyn_cast<CallInst>(&I)) {
          if (call->isInlineAsm()) continue;
          auto targetFuncType = call->getFunctionType();
          if (targetFuncType->isVarArg()) {
            // TODO: Remove this limitation
            //       This is a restriction of the backend, I think.
            if (not targetFuncType->getReturnType()->isVoidTy()) continue;
          }
          if (auto func = call->getCalledFunction(); func != nullptr) {
            if (func->getName().startswith("alaska.")) continue;
            if (func->getName().startswith("__alaska")) continue;
          }
          statepointCalls.insert(call);
          for (auto &tr : translations) {
            mustTrack.insert(tr.get());
            if (tr->isLive(call)) {
              mustTrack.insert(tr.get());
            }
          }
        }
      }
    }



    // If there are no translations in the mustTranslate set,
    // give up. This function doesn't need tracking added! Yippee!
    if (mustTrack.size() == 0) continue;


    // TODO: put this in the right place!
    F.setGC("coreclr");




    // Given a lock, which cell does it belong to? (eagerly)
    std::map<alaska::Translation *, long> lock_cell_ids;
    // Interference - a mapping from locks to the locks it is alive along side of.
    std::map<alaska::Translation *, std::set<alaska::Translation *>> interference;

    // Loop over each translation, then over it's live instructions. For each live instruction see
    // which other locks are live in those instructions. This is horrible and slow. (but works)
    for (auto &first : mustTrack) {
      lock_cell_ids[first] = -1;
      // A lock interferes with itself
      interference[first].insert(first);
      for (auto b1 : first->liveBlocks) {
        for (auto &second : mustTrack) {
          if (second != first && second->isLive(b1)) {
            // interference!
            interference[first].insert(second);
            interference[second].insert(first);
          }
        }
      }
    }

    // The algorithm we use is a simple greedy algo. We don't need to do a fancy graph
    // coloring allocation here, as we don't need to worry about register spilling
    // (we can just make more "registers" instead of spilling). All cells are equal as well,
    // so there aren't any restrictions on which lock can get which cell.
    for (auto &[lock, intr] : interference) {
      long available_cell = -1;
      llvm::SparseBitVector<128> unavail;
      for (auto other : intr) {
        long cell = lock_cell_ids[other];
        if (cell != -1) {
          unavail.set(cell);
        }
      }
      // find a cell that hasn't been used by any of the interfering locks.
      for (long current_cell = 0;; current_cell++) {
        if (!unavail.test(current_cell)) {
          available_cell = current_cell;
          break;
        }
      }
      lock_cell_ids[lock] = available_cell;
    }

    // {
    //   long ind = 0;
    //   for (auto tr : mustTrack) {
    //     lock_cell_ids[tr] = ind++;
    //   }
    // }

    // Figure out the max available cell
    long max_cell = 0;  // the maximum level of interference
    for (auto &[_, i] : lock_cell_ids) {
      if (max_cell < i) max_cell = i;
    }

    max_cell++;  // handle zero index vs. size.
    // long max_cell = mustTrack.size() + 1;

    auto pointerType = llvm::PointerType::get(M.getContext(), 0);
    auto *arrayType = ArrayType::get(pointerType, max_cell);

    IRBuilder<> atEntry(F.front().getFirstNonPHI());


    auto trackSet = atEntry.CreateAlloca(arrayType, nullptr, "localPinSet");
    // atEntry.CreateStore(ConstantAggregateZero::get(arrayType), trackSet, true);


    alaska::println(mustTrack.size(), "\t", max_cell, "\t", F.getName());

    for (auto *tr : mustTrack) {
      // (void)tr;
      // char buf[512];
      long ind = lock_cell_ids[tr];
      // snprintf(buf, 512, "pinCell.%ld", ind);
      IRBuilder<> b(tr->translation);
      auto cell = CreateGEP(M.getContext(), b, arrayType, trackSet, ind, "");
      auto handle = tr->getHandle();
      b.CreateStore(handle, cell, true);
    }


    for (auto &call : statepointCalls) {
      IRBuilder<> b(call);
      std::vector<llvm::Value *> callArgs(call->args().begin(), call->args().end());
      std::vector<llvm::Value *> gcArgs;
      gcArgs.push_back(ConstantInt::get(IntegerType::get(M.getContext(), 64), mustTrack.size()));
      gcArgs.push_back(trackSet);

      Optional<ArrayRef<Value *>> deoptArgs(gcArgs);

      auto called = call->getCalledOperand();
      int patch_size = 0;
      int id = 0xABCDEF00;
      if (auto func = call->getCalledFunction()) {
        if (func->getName() == "alaska_barrier_poll") {
          id = 'PATC';
          patch_size = 8;
        }
      }

      // Value * > GCArgs, const Twine &Name="")
      auto token = b.CreateGCStatepointCall(id, patch_size,
          llvm::FunctionCallee(call->getFunctionType(), called), callArgs, deoptArgs, {},
          "statepoint");
      auto result = b.CreateGCResult(token, call->getType());
      call->replaceAllUsesWith(result);
      call->eraseFromParent();
    }
  }
#endif



  // return PreservedAnalyses::none();

#if 0


  for (auto &F : M) {
    // Ignore functions with no bodies
    if (F.empty()) continue;
    // Extract all the locks from the function
    auto translations = alaska::extractTranslations(F);
    // If the function had no locks, don't do anything
    if (translations.empty()) continue;
    llvm::DenseMap<CallInst *, std::set<alaska::Translation *>> liveTranslations;
    for (auto &BB : F) {
      F.setGC("coreclr");
      for (auto &I : BB) {
        if (auto *call = dyn_cast<IntrinsicInst>(&I)) {
          continue;
        }
        if (auto *call = dyn_cast<CallInst>(&I)) {
          if (call->isInlineAsm()) continue;
          auto targetFuncType = call->getFunctionType();
          if (targetFuncType->isVarArg()) {
            // TODO: Remove this limitation
            //       This is a restriction of the backend, I think.
            if (not targetFuncType->getReturnType()->isVoidTy()) continue;
          }
          if (auto func = call->getCalledFunction(); func != nullptr) {
            if (func->getName().startswith("alaska.")) continue;
            if (func->getName().startswith("__alaska")) continue;
          }
          // access the call so it exists.
          liveTranslations[call];
          for (auto &tr : translations) {
            if (tr->isLive(call)) {
              liveTranslations[call].insert(tr.get());
            }
          }
        }
      }
    }

    IRBuilder<> atEntry(F.front().getFirstNonPHI());


    std::vector<Type *> EltTys;
    auto pointerType = llvm::PointerType::get(M.getContext(), 0);
    // auto i64Ty = IntegerType::get(M.getContext(), 64);
    auto stackEntryTy = StructType::create(M.getContext(), "alaska_pin_set_entry");
    EltTys.clear();
    EltTys.push_back(pointerType);  // ptr
    stackEntryTy->setBody(EltTys);
    auto a = atEntry.CreateAlloca(stackEntryTy, nullptr, "localPinSet");


    for (auto &[call, translations] : liveTranslations) {
      IRBuilder<> b(call);
      std::vector<llvm::Value *> callArgs(call->args().begin(), call->args().end());
      std::vector<llvm::Value *> gcArgs;
      gcArgs.push_back(ConstantInt::get(IntegerType::get(M.getContext(), 64), 42));
      gcArgs.push_back(a);
      // for (auto *tr : translations) {
      //   gcArgs.push_back(tr->getHandle());
      // }
      Optional<ArrayRef<Value *>> deoptArgs(gcArgs);
      alaska::println(translations.size(), "\t", *call);

      auto called = call->getCalledOperand();
      int patch_size = 0;
      int id = 0xABCDEF00;
      if (auto func = call->getCalledFunction()) {
        if (func->getName() == "alaska_barrier_poll") {
          id = 'PATC';
          patch_size = 8;
        }
      }

      // Value * > GCArgs, const Twine &Name="")
      auto token = b.CreateGCStatepointCall(id, patch_size,
          llvm::FunctionCallee(call->getFunctionType(), called), callArgs, deoptArgs, {},
          "statepoint");
      auto result = b.CreateGCResult(token, call->getType());
      call->replaceAllUsesWith(result);
      call->eraseFromParent();
    }
  }
#endif


// Define this to enable the old lock system.
// #define OLD_LOCK
#ifdef OLD_LOCK
  std::vector<Type *> EltTys;

  auto pointerType = llvm::PointerType::get(M.getContext(), 0);
  auto i64Ty = IntegerType::get(M.getContext(), 64);
  auto stackEntryTy = StructType::create(M.getContext(), "alaska_stackentry");
  EltTys.clear();
  EltTys.push_back(PointerType::getUnqual(stackEntryTy));  // ptr
  EltTys.push_back(PointerType::getUnqual(stackEntryTy));  // ptr
  EltTys.push_back(i64Ty);                                 // i64
  stackEntryTy->setBody(EltTys);

  // Get the root chain if it already exists.
  auto Head = M.getGlobalVariable("alaska_lock_root_chain");
  if (Head == nullptr) {
    Head = new GlobalVariable(M, pointerType, false, GlobalValue::ExternalLinkage,
        Constant::getNullValue(pointerType), "alaska_lock_root_chain", nullptr,
        llvm::GlobalValue::InitialExecTLSModel);
  }

  Head->setThreadLocal(true);
  Head->setLinkage(GlobalValue::ExternalWeakLinkage);
  Head->setInitializer(NULL);


  for (auto &F : M) {
    // Ignore functions with no bodies
    if (F.empty()) continue;


    // alaska::println("inserting locks into ", F.getName());
    // Extract all the locks from the function
    auto translations = alaska::extractTranslations(F);
    // If the function had no locks, don't do anything
    if (translations.empty()) continue;

    F.setGC("coreclr");

    // Given a lock, which cell does it belong to? (eagerly)
    std::map<alaska::Translation *, long> lock_cell_ids;
    // Interference - a mapping from locks to the locks it is alive along side of.
    std::map<alaska::Translation *, std::set<alaska::Translation *>> interference;

    // Loop over each translation, then over it's live instructions. For each live instruction see
    // which other locks are live in those instructions. This is horrible and slow. (but works)
    for (auto &first : translations) {
      lock_cell_ids[first.get()] = -1;
      // A lock interferes with itself
      interference[first.get()].insert(first.get());
      for (auto b1 : first->liveBlocks) {
        for (auto &second : translations) {
          if (second != first && second->isLive(b1)) {
            // interference!
            interference[first.get()].insert(second.get());
            interference[second.get()].insert(first.get());
          }
        }
      }
    }

    // The algorithm we use is a simple greedy algo. We don't need to do a fancy graph
    // coloring allocation here, as we don't need to worry about register spilling
    // (we can just make more "registers" instead of spilling). All cells are equal as well,
    // so there aren't any restrictions on which lock can get which cell.
    for (auto &[lock, intr] : interference) {
      long available_cell = -1;
      llvm::SparseBitVector<128> unavail;
      for (auto other : intr) {
        long cell = lock_cell_ids[other];
        if (cell != -1) {
          unavail.set(cell);
        }
      }
      // find a cell that hasn't been used by any of the interfering locks.
      for (long current_cell = 0;; current_cell++) {
        if (!unavail.test(current_cell)) {
          available_cell = current_cell;
          break;
        }
      }
      lock_cell_ids[lock] = available_cell;
    }

    // Figure out the max available cell
    long max_cell = 0;  // the maximum level of interference
    for (auto &[_, i] : lock_cell_ids) {
      if (max_cell < i) max_cell = i;
    }

    long cell_count = max_cell + 1;  // account for 0 index

    // fprintf(stderr, "%3ld dynamic cells required for %zu static translations in %s\n",
    // cell_count,
    //     translations.size(), F.getName().data());

    // Create the type that will go on the stack.
    EltTys.clear();
    EltTys.push_back(stackEntryTy);
    // enough spots for each pointerType
    for (long i = 0; i < cell_count; i++) {
      EltTys.push_back(pointerType);
    }
    auto *concreteStackEntryTy =
        StructType::create(EltTys, ("alaska_stackentry." + F.getName()).str());

    IRBuilder<> atEntry(F.front().getFirstNonPHI());
    auto *stackEntry = atEntry.CreateAlloca(concreteStackEntryTy, nullptr, "lockStackFrame");
    atEntry.CreateStore(ConstantAggregateZero::get(concreteStackEntryTy), stackEntry, true);

    auto *FunctionPtr =
        CreateGEP(M.getContext(), atEntry, stackEntryTy, stackEntry, 1, "lockStackFrame.func");
    atEntry.CreateStore(&F, FunctionPtr, true);

    auto *EntryCountPtr =
        CreateGEP(M.getContext(), atEntry, stackEntryTy, stackEntry, 2, "lockStackFrame.count");
    // cur->count = N;
    atEntry.CreateStore(ConstantInt::get(i64Ty, cell_count), EntryCountPtr, true);
    // cur->prev = head;
    auto *CurrentHead =
        atEntry.CreateLoad(stackEntryTy->getPointerTo(), Head, "lockStackCurrentHead");
    atEntry.CreateStore(CurrentHead, stackEntry, true);
    // head = cur;
    atEntry.CreateStore(stackEntry, Head, true);


    std::vector<GetElementPtrInst *> lockCells;
    for (long ind = 0; ind < cell_count; ind++) {
      char buf[512];
      snprintf(buf, 512, "lockCell.%ld", ind);

      auto cell =
          CreateGEP(M.getContext(), atEntry, concreteStackEntryTy, stackEntry, ind + 1, buf);
      atEntry.CreateStore(
          llvm::ConstantPointerNull::get(PointerType::get(M.getContext(), 0)), cell, true);
      lockCells.push_back(cell);
    }

    for (auto &translation : translations) {
      auto handle = translation->getHandle();

      IRBuilder<> b(translation->translation);

      long ind = lock_cell_ids[translation.get()];
      auto *cell = lockCells[ind];

      b.SetInsertPoint(translation->translation);
      b.CreateStore(handle, cell, false);

      // Insert a store right after all the releases to say "we're done with this handle".
      for (auto unlock : translation->releases) {
        b.SetInsertPoint(unlock->getNextNode());
        b.CreateStore(
            llvm::ConstantPointerNull::get(dyn_cast<PointerType>(handle->getType())), cell, true);
      }
    }

    // For each instruction that escapes...
    EscapeEnumerator EE(F, "alaska_cleanup", /*HandleExceptions=*/false, nullptr);
    while (IRBuilder<> *AtExit = EE.Next()) {
      // Pop the entry from the shadow stack. Don't reuse CurrentHead from
      // AtEntry, since that would make the value live for the entire function.
      Value *SavedHead =
          AtExit->CreateLoad(stackEntryTy->getPointerTo(), stackEntry, "alaska_savedhead");
      AtExit->CreateStore(SavedHead, Head);
      // AtExit->CreateStore(CurrentHead, Head);
    }
  }
#endif

  return PreservedAnalyses::none();
}
