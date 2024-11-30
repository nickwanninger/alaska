#include <alaska/Passes.h>
#include <alaska/Translations.h>
#include <alaska/Utils.h>
#include <optional>


#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/Transforms/Utils/EscapeEnumerator.h"
#include "llvm/IR/PassManager.h"
#include "llvm/ADT/SparseBitVector.h"
#include "llvm/IR/Verifier.h"

// #include "llvm/IR/AttributeMask.h"
#include "llvm/IR/Attributes.h"


using namespace llvm;


// List of all function attributes which must be stripped when lowering from
// abstract machine model to physical machine model.  Essentially, these are
// all the effects a safepoint might have which we ignored in the abstract
// machine model for purposes of optimization.  We have to strip these on
// both function declarations and call sites.
static constexpr Attribute::AttrKind FnAttrsToStrip[] = {
    Attribute::Memory, Attribute::NoSync, Attribute::NoFree};

// Create new attribute set containing only attributes which can be transferred
// from original call to the safepoint.
static AttributeList legalizeCallAttributes(
    LLVMContext &Ctx, AttributeList OrigAL, AttributeList StatepointAL, int argc) {
  if (OrigAL.isEmpty()) return StatepointAL;

  // Remove the readonly, readnone, and statepoint function attributes.
  AttrBuilder FnAttrs(Ctx, OrigAL.getFnAttrs());
  for (auto Attr : FnAttrsToStrip) {
    FnAttrs.removeAttribute(Attr);
  }

  // for (Attribute A : OrigAL.getFnAttrs()) {
  //   if (isStatepointDirectiveAttr(A)) FnAttrs.removeAttribute(A);
  // }

  // Just skip parameter and return attributes for now
  StatepointAL = StatepointAL.addFnAttributes(Ctx, FnAttrs);

  // for (auto Attr : FnAttrsToStrip) {
  //   FnAttrs.removeAttribute(Attr);
  // }

  for (int ind = 0; ind < argc; ind++) {
    AttrBuilder ParamAttrs(Ctx, OrigAL.getParamAttrs(ind));
    ParamAttrs.removeAttribute(Attribute::AttrKind::StructRet);
    ParamAttrs.removeAttribute(Attribute::AttrKind::AllocSize);
    StatepointAL = StatepointAL.addParamAttributes(Ctx, ind + 5, ParamAttrs);
  }


  return StatepointAL;
}



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
    F.setSection(".text.alaska");

    // Extract all the translations from the function
    auto translations = alaska::extractTranslations(F);

    // The set of translations that *must* be translated.
    std::set<alaska::Translation *> mustTrack;
    // A set of all
    std::set<CallBase *> statepointCalls;


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
            if (func->getName() == "alaska_do_handle_fault_check") continue;
          }
          statepointCalls.insert(call);
        }

        if (auto *invoke = dyn_cast<InvokeInst>(&I)) {
          if (invoke->isInlineAsm()) continue;
          auto targetFuncType = invoke->getFunctionType();
          if (targetFuncType->isVarArg()) {
            // TODO: Remove this limitation
            //       This is a restriction of the backend, I think.
            if (not targetFuncType->getReturnType()->isVoidTy()) continue;
          }
          if (auto func = invoke->getCalledFunction(); func != nullptr) {
            if (func->getName().startswith("alaska.")) continue;
          }
          statepointCalls.insert(invoke);
        }
      }
    }


    for (auto &tr : translations) {
      for (auto &sp : statepointCalls) {
        if (tr->isLive(sp)) {
          mustTrack.insert(tr.get());
          break;
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


    llvm::AllocaInst *trackSet = NULL;

    long max_cell = mustTrack.size() + 1;

    auto pointerType = llvm::PointerType::get(M.getContext(), 0);
    auto *arrayType = ArrayType::get(pointerType, max_cell);

    IRBuilder<> atEntry(F.front().getFirstNonPHI());
    if (mustTrack.size() > 0) {
      trackSet = atEntry.CreateAlloca(arrayType, nullptr, "localPinSet");
    }
    for (auto *tr : mustTrack) {
      long ind = pin_cell_ids[tr];
      IRBuilder<> b(tr->translation);
      auto cell = CreateGEP(M.getContext(), b, arrayType, trackSet, ind, "");
      auto handle = tr->getHandle();
      b.CreateStore(handle, cell, false);
    }


    for (auto &call : statepointCalls) {
      IRBuilder<> b(call);
      std::vector<llvm::Value *> callArgs(call->args().begin(), call->args().end());
      std::vector<llvm::Value *> gcArgs;
      if (trackSet != NULL) {
        gcArgs.push_back(ConstantInt::get(IntegerType::get(M.getContext(), 64), mustTrack.size()));
        gcArgs.push_back(trackSet);
      }

      std::optional<ArrayRef<Value *>> deoptArgs(gcArgs);

      auto called = call->getCalledOperand();
      int patch_size = 0;
      int id = 0;

      if (auto func = call->getCalledFunction()) {
        if (func->getName() == "alaska_barrier_poll") {
          id = 'PATC';
          patch_size = ALASKA_PATCH_SIZE;  // TODO: handle ARM
        } else if (func->hasFnAttribute("alaska_mightblock")) {
          id = 'BLOK';  // This function might block! Record it in the stackmap
          patch_size = 0;
        } else if (func->getName() == "alaska_do_handle_fault_check") {
          id = 'FALT';
          patch_size = ALASKA_PATCH_SIZE;  // TODO: handle ARM
        }
      }



      llvm::CallBase *token;
      auto callTarget = llvm::FunctionCallee(call->getFunctionType(), called);

      if (auto invoke = dyn_cast<InvokeInst>(call)) {
        // alaska::println("invoke:", *invoke);
        auto *statepointInvoke = b.CreateGCStatepointInvoke(id, patch_size, callTarget,
            invoke->getNormalDest(), invoke->getUnwindDest(), callArgs, deoptArgs, {}, "");
        // alaska::println("new:", *statepointInvoke);

        statepointInvoke->setCallingConv(invoke->getCallingConv());

        // Currently we will fail on parameter attributes and on certain
        // function attributes.  In case if we can handle this set of attributes -
        // set up function attrs directly on statepoint and return attrs later for
        // gc_result intrinsic.
        statepointInvoke->setAttributes(legalizeCallAttributes(invoke->getContext(),
            invoke->getAttributes(), statepointInvoke->getAttributes(), invoke->arg_size()));


        BasicBlock *normalDest = statepointInvoke->getNormalDest();

        auto trampoline = llvm::SplitEdge(statepointInvoke->getParent(), normalDest);
        statepointInvoke->setNormalDest(trampoline);

        token = statepointInvoke;

        trampoline->setName("invoke_statepoint_tramp");

        b.SetInsertPoint(&*trampoline->getFirstInsertionPt());
        b.SetCurrentDebugLocation(invoke->getDebugLoc());

        if (!call->getType()->isVoidTy()) {
          auto result = b.CreateGCResult(token, call->getType());
          result->setAttributes(AttributeList::get(result->getContext(), AttributeList::ReturnIndex,
              invoke->getAttributes().getRetAttrs()));

          call->replaceAllUsesWith(result);
        }
      } else if (auto *CI = dyn_cast<CallInst>(call)) {
        auto SPCall =
            b.CreateGCStatepointCall(id, patch_size, callTarget, callArgs, deoptArgs, {}, "");

        SPCall->setTailCallKind(CI->getTailCallKind());
        SPCall->setCallingConv(CI->getCallingConv());
        SPCall->setAttributes(legalizeCallAttributes(
            CI->getContext(), CI->getAttributes(), SPCall->getAttributes(), CI->arg_size()));



        token = SPCall;
        if (!call->getType()->isVoidTy()) {
          auto result = b.CreateGCResult(token, call->getType());

          result->setAttributes(AttributeList::get(result->getContext(), AttributeList::ReturnIndex,
              call->getAttributes().getRetAttrs()));
          call->replaceAllUsesWith(result);
        }
      } else {
        abort();
      }




      // If the safepoint is a PATCH safepoint, it won't ever be a call. It will always be a "NOP"
      // style instruction and will never corrupt any registers. As such, to reduce the overhead of
      // safepoints (to allow backend optimizations and whatnot) we mark the call of such locations
      // as "preserve all", meaning the caller does not need to stash any registers on the stack. In
      // the common case, this means patch points have basically no overhead outside of I$ lookups
      // :)
      //
      // For reference, if this line is removed, NAS cg.B running on 64 cores under OpenMP sees
      // overhead increase from 1.02x to 4x!
      if (id == 'PATC' or id == 'FALT') {
        token->setCallingConv(CallingConv::PreserveAll);
      }


      call->eraseFromParent();
    }

    if (verifyFunction(F, &errs())) {
      errs() << "Function verification failed!\n";
      errs() << F.getName() << "\n";
      errs() << F << "\n";
      exit(EXIT_FAILURE);
    }
  }

  return PreservedAnalyses::none();
}



static void removeAllArgumentsFromFunction(llvm::Function *oldFunction) {
  if (oldFunction == nullptr) return;

  llvm::Module *module = oldFunction->getParent();


  // Create the new function type with no arguments
  llvm::FunctionType *newFunctionType =
      llvm::FunctionType::get(oldFunction->getReturnType(), {}, oldFunction->isVarArg());


  std::string name = oldFunction->getName().str();
  // Create the new function
  llvm::Function *newFunction = llvm::Function::Create(
      newFunctionType, oldFunction->getLinkage(), oldFunction->getName(), module);

  // Map old arguments to no arguments (leave body unchanged, arguments will no longer be valid)
  llvm::ValueToValueMapTy valueMap;
  oldFunction->replaceAllUsesWith(newFunction);
  // Remove the old function if it's no longer used

  oldFunction->setName(name + "_old");
  // oldFunction->eraseFromParent();
  newFunction->setName(name);
}



PreservedAnalyses HandleFaultPass::run(Module &M, ModuleAnalysisManager &AM) {
  // removeAllArgumentsFromFunction(M.getFunction("alaska_do_handle_fault_check"));

  llvm::FunctionType *faultFunctionType =
      llvm::FunctionType::get(llvm::Type::getVoidTy(M.getContext()), {}, false);

  auto faultFunction = llvm::Function::Create(
      faultFunctionType, llvm::GlobalValue::LinkageTypes::ExternalLinkage, "alaska.fault", M);

  for (auto &F : M) {
    // Ignore functions with no bodies
    if (F.empty()) continue;

    F.setGC("coreclr");

    std::set<CallBase *> faultCalls;

    for (auto &BB : F) {
      for (auto &I : BB) {
        if (auto *call = dyn_cast<CallInst>(&I)) {
          // alaska::println("  call: ", *call, call->getCalledFunction());
          if (auto func = call->getCalledFunction()) {
            if (func->getName() == "alaska_do_handle_fault_check") {
              faultCalls.insert(call);
            }
          }
        }
      }
    }

    // Given a translation, which cell does it belong to? (eagerly)
    std::map<alaska::Translation *, long> pin_cell_ids;
    // Interference - a mapping from translations to the translations it is alive along side of.
    std::map<alaska::Translation *, std::set<alaska::Translation *>> interference;



    for (auto &call : faultCalls) {
      IRBuilder<> b(call);
      // std::vector<llvm::Value *> callArgs(call->args().begin(), call->args().end());
      std::vector<llvm::Value *> callArgs;
      std::vector<llvm::Value *> gcArgs(call->args().begin(), call->args().end());

      std::optional<ArrayRef<Value *>> deoptArgs(gcArgs);

      int patch_size = 0;
      int id = 0;

      id = 'HFLT';
      patch_size = 4;  // TODO: handle GENERALITY

      // auto callTarget = llvm::FunctionCallee(call->getFunctionType(), called);
      auto callTarget = llvm::FunctionCallee(faultFunctionType, faultFunction);

      auto *CI = dyn_cast<CallInst>(call);
      auto *SPCall =
        b.CreateGCStatepointCall(id, patch_size, callTarget, callArgs, deoptArgs, gcArgs, "");

      // SPCall->setTailCallKind(CI->getTailCallKind());
      SPCall->setCallingConv(CallingConv::PreserveAll);
      // SPCall->setAttributes(legalizeCallAttributes(
      //     CI->getContext(), CI->getAttributes(), SPCall->getAttributes(), CI->arg_size()));

      call->eraseFromParent();
      alaska::println(*SPCall);
    }


    if (verifyFunction(F, &errs())) {
      errs() << "Function verification failed!\n";
      errs() << F.getName() << "\n";
      errs() << F << "\n";
      exit(EXIT_FAILURE);
    }
    // errs() << F << "\n";
  }

  return PreservedAnalyses::none();
}
