#include <alaska/Passes.h>
#include <alaska/Translations.h>
#include <alaska/Utils.h>

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
#ifndef ALASKA_LOCK_TRACKING
  return PreservedAnalyses::all();
#endif
  std::vector<Type *> EltTys;

  auto pointerType = llvm::PointerType::get(M.getContext(), 0);
  auto i64Ty = IntegerType::get(M.getContext(), 64);
  auto stackEntryTy = StructType::create(M.getContext(), "alaska_stackentry");
  EltTys.clear();
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

  // TODO: instead of getting this from a global variable, it would be interesting to see how
  // versioning functions with an additional argument containing the root chain might improve
  // performance for "short" functions.  Then, when you call a function it does not have to look up
  // the address of the thread-local pointer. Effectively, we would take this code:
  //
  //     void write_ptr(int *x) {
  //       root_chain = ...
  //       *x = 0;
  //     }
  //     and convert it to this code:
  //     void write_ptr(int *x) {
  //       root_chain = ...
  //       write_ptr.rc(x, root_chain);
  //     }
  //     void write_ptr.rc(int *x, root_chain *rc) {
  //       *x = 0;
  //     }
  //
  // That way, functions would simply take the pointer to the current root chain as a function,
  // which would reduce prelude overhead significantly on ARM. If the old function was called
  // without passing the root chain, it would simply load the location and call into the versioned
  // function with that pointer.


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

    // Given a lock, which cell does it belong to? (eagerly)
    std::map<alaska::Translation *, long> lock_cell_ids;
    // Interference - a mapping from locks to the locks it is alive along side of.
    std::map<alaska::Translation *, std::set<alaska::Translation *>> interference;

    // alaska::println(translations.size(), " translations.");

    // Loop over each translation, then over it's live instructions. For each live instruction see
    // which other locks are live in those instructions. This is horrible and slow. (but works)
    for (auto &first : translations) {
      lock_cell_ids[first.get()] = -1;
      // A lock interferes with itself
      interference[first.get()].insert(first.get());
      for (auto inst : first->liveInstructions) {
        for (auto &second : translations) {
          if (second != first && second->isLive(inst)) {
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

    // fprintf(stderr, "%3ld dynamic cells required for %zu static translations in %s\n", cell_count,
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


    auto *EntryCountPtr =
        CreateGEP(M.getContext(), atEntry, stackEntryTy, stackEntry, 1, "lockStackFrame.count");
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
      // for (auto unlock : translation->releases) {
      //   b.SetInsertPoint(unlock->getNextNode());
      //   b.CreateStore(
      //       llvm::ConstantPointerNull::get(dyn_cast<PointerType>(handle->getType())), cell,
      //       true);
      // }
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

  return PreservedAnalyses::none();
}
