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

PreservedAnalyses PinTrackingPass::run(Module &M, ModuleAnalysisManager &AM) {
  for (auto &F : M) {
    // Ignore functions with no bodies
    if (F.empty()) continue;

    // If the function is simple, it can't poll, so we can skip it! Yippee!
    if (F.hasFnAttribute("alaska_is_simple")) {
      continue;
    }

    F.setGC("coreclr");

    // Extract all the translations from the function
    auto translations = alaska::extractTranslations(F);
    // If the function had no translations, don't do anything
    // if (translations.empty()) continue;

    // The set of translations that *must* be translated.
    std::set<alaska::Translation *> mustTrack;
    // A set of all
    std::set<CallInst *> statepointCalls;

    for (auto &tr : translations) {
      mustTrack.insert(tr.get());
    }

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
            // if (func->getName().startswith("__alaska")) continue;
          }
          statepointCalls.insert(call);
          // for (auto &tr : translations) {
          //   mustTrack.insert(tr.get());
          //   if (tr->isLive(call)) {
          //     mustTrack.insert(tr.get());
          //   }
          // }
        }
      }
    }



    // Given a translation, which cell does it belong to? (eagerly)
    std::map<alaska::Translation *, long> pin_cell_ids;
    // Interference - a mapping from translations to the translations it is alive along side of.
    std::map<alaska::Translation *, std::set<alaska::Translation *>> interference;

    // Loop over each translation, then over it's live instructions. For each live instruction see
    // which other translations are live in those instructions. This is horrible and slow. (but
    // works)
    for (auto &first : mustTrack) {
      pin_cell_ids[first] = -1;
      // A translation interferes with itself
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
    // so there aren't any restrictions on which translation can get which pin cell.
    for (auto &[tr, intr] : interference) {
      long available_cell = -1;
      llvm::SparseBitVector<128> unavail;
      for (auto other : intr) {
        long cell = pin_cell_ids[other];
        if (cell != -1) {
          unavail.set(cell);
        }
      }
      // find a cell that hasn't been used by any of the interfering translations.
      for (long current_cell = 0;; current_cell++) {
        if (!unavail.test(current_cell)) {
          available_cell = current_cell;
          break;
        }
      }
      pin_cell_ids[tr] = available_cell;
    }

    long ind = 0;
    for (auto tr : mustTrack) {
      pin_cell_ids[tr] = ind++;
    }

    llvm::AllocaInst *trackSet = NULL;

    long max_cell = mustTrack.size() + 1;

    auto pointerType = llvm::PointerType::get(M.getContext(), 0);
    auto *arrayType = ArrayType::get(pointerType, max_cell);

    IRBuilder<> atEntry(F.front().getFirstNonPHI());
    trackSet = atEntry.CreateAlloca(arrayType, nullptr, "localPinSet");

    for (auto *tr : mustTrack) {
      long ind = pin_cell_ids[tr];
      IRBuilder<> b(tr->translation);
      auto cell = CreateGEP(M.getContext(), b, arrayType, trackSet, ind, "");
      auto handle = tr->getHandle();
      b.CreateStore(handle, cell, true);

      for (auto rel : tr->releases) {
        b.SetInsertPoint(rel->getNextNode());
        b.CreateStore(Constant::getNullValue(pointerType), cell, false);
      }
    }


    for (auto &call : statepointCalls) {
      IRBuilder<> b(call);
      std::vector<llvm::Value *> callArgs(call->args().begin(), call->args().end());
      std::vector<llvm::Value *> gcArgs;
      if (trackSet != NULL) {
        gcArgs.push_back(ConstantInt::get(IntegerType::get(M.getContext(), 64), mustTrack.size()));
        gcArgs.push_back(trackSet);
      }

      Optional<ArrayRef<Value *>> deoptArgs(gcArgs);

      auto called = call->getCalledOperand();
      int patch_size = 0;
      int id = 0xABCDEF00;
      if (auto func = call->getCalledFunction()) {
        if (func->getName() == "alaska_barrier_poll") {
          id = 'PATC';
          patch_size = ALASKA_PATCH_SIZE;  // TODO: handle ARM
        }
      }

      // Value * > GCArgs, const Twine &Name="")
      auto token = b.CreateGCStatepointCall(id, patch_size,
          llvm::FunctionCallee(call->getFunctionType(), called), callArgs, deoptArgs, {}, "");
      auto result = b.CreateGCResult(token, call->getType());
      call->replaceAllUsesWith(result);
      call->eraseFromParent();
    }
  }

  return PreservedAnalyses::none();
}
