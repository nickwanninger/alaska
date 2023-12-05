#include <alaska/Passes.h>
#include <alaska/Translations.h>
#include <alaska/Utils.h>
#include <llvm/IR/InstVisitor.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <set>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/GetElementPtrTypeIterator.h>
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/PointerUnion.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/User.h"
#include "llvm/Support/Casting.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iterator>

using namespace llvm;


std::vector<llvm::CallBase *> collectCalls(llvm::Module &M, const char *name) {
  std::vector<llvm::CallBase *> calls;

  if (auto func = M.getFunction(name)) {
    for (auto user : func->users()) {
      if (auto call = dyn_cast<CallBase>(user)) {
        calls.push_back(call);
      }
    }
  }

  return calls;
}

bool collectOffsets(GetElementPtrInst *gep, const DataLayout &DL, unsigned BitWidth,
    std::unordered_map<Value *, APInt> &VariableOffsets, APInt &ConstantOffset) {
  // assert(BitWidth == dl.getindexsizeinbits(gp.getpointeraddressspace()) &&
  //      "The offset bit width does not match DL specification.");

  auto CollectConstantOffset = [&](APInt Index, uint64_t Size) {
    Index = Index.sextOrTrunc(BitWidth);
    APInt IndexedSize = APInt(BitWidth, Size);
    ConstantOffset += Index * IndexedSize;
  };

  llvm::gep_type_iterator GTI = llvm::gep_type_begin(gep);

  for (unsigned I = 1, E = gep->getNumOperands(); I != E; ++I, ++GTI) {
    // Scalable vectors are multiplied by a runtime constant.
    bool ScalableType = isa<VectorType>(GTI.getIndexedType());

    Value *V = GTI.getOperand();
    StructType *STy = GTI.getStructTypeOrNull();
    // Handle ConstantInt if possible.
    if (auto ConstOffset = dyn_cast<ConstantInt>(V)) {
      // If the type is scalable and the constant is not zero (vscale * n * 0 =
      // 0) bailout.
      // TODO: If the runtime value is accessible at any point before DWARF
      // emission, then we could potentially keep a forward reference to it
      // in the debug value to be filled in later.
      if (ScalableType) return false;
      // Handle a struct index, which adds its field offset to the pointer.
      if (STy) {
        unsigned ElementIdx = ConstOffset->getZExtValue();
        const StructLayout *SL = DL.getStructLayout(STy);
        // Element offset is in bytes.
        CollectConstantOffset(APInt(BitWidth, SL->getElementOffset(ElementIdx)), 1);
        continue;
      }
      CollectConstantOffset(ConstOffset->getValue(), DL.getTypeAllocSize(GTI.getIndexedType()));
      continue;
    }

    if (STy) {
      // errs()<<"Found gep value which is  struct type and not a constant\n";
      return false;
    }
    if (ScalableType) {
      // errs()<<"Found gep value which is  vector type and is not a constant\n";
      return false;
    }
    APInt IndexedSize = APInt(BitWidth, DL.getTypeAllocSize(GTI.getIndexedType()));
    // Insert an initial offset of 0 for V iff none exists already, then
    // increment the offset by IndexedSize.
    // checks if Indexed Size is zero LLVM9 APIINT
    if (!IndexedSize) {
    } else {
      VariableOffsets.insert({V, APInt(BitWidth, 0)});
      VariableOffsets[V] += IndexedSize;
    }
  }
  return true;
}


llvm::PreservedAnalyses AlaskaLowerPass::run(llvm::Module &M, llvm::ModuleAnalysisManager &AM) {
  std::set<llvm::Instruction *> to_delete;

  llvm::DataLayout DL(&M);
  // auto bitWidth = 8;  // I think. (64 bit)

  // Lower GC rooting.
  if (auto func = M.getFunction("alaska.safepoint_poll")) {
    auto pollFunc = M.getOrInsertFunction("alaska_safepoint", func->getFunctionType());
    for (auto call : collectCalls(M, "alaska.safepoint_poll")) {
      call->setCalledFunction(pollFunc);
    }
  }

  // Lower alaska.root
  for (auto *call : collectCalls(M, "alaska.root")) {
    IRBuilder<> b(call);  // insert after the call
                          //
    // auto cast = b.CreateAddrSpaceCast(call->getArgOperand(0), PointerType::get(M.getContext(),
    // 0)); call->replaceAllUsesWith(cast);
    call->replaceAllUsesWith(call->getArgOperand(0));
    to_delete.insert(call);
    if (auto *invoke = dyn_cast<llvm::InvokeInst>(call)) {
      auto *landing_pad = invoke->getLandingPadInst();
      to_delete.insert(landing_pad);
    }
  }




  // Lower alaska.translate
  if (auto func = M.getFunction("alaska.translate")) {
    auto translateFunc = M.getOrInsertFunction("alaska_translate", func->getFunctionType());
    auto translateEscapeFunc =
        M.getOrInsertFunction("alaska_translate_escape", func->getFunctionType());
    // auto translateFuncUnCond =
    //     M.getOrInsertFunction("alaska_translate_uncond", func->getFunctionType());

    for (auto *call : collectCalls(M, "alaska.translate")) {
      IRBuilder<> b(call);  // insert after the call

      // If the return value is only used in calls, run the more expensive "escape" variant
      bool onlyCalls = true;
      for (auto *user : call->users()) {
        if (auto c = dyn_cast<CallInst>(user)) {
          // Good!
        } else {
          // onlyCalls = false;
        }
      }
      onlyCalls = false;
      if (onlyCalls) {
        call->setCalledFunction(translateEscapeFunc);
      } else {
        call->setCalledFunction(translateFunc);
      }
    }
  }



  // Lower alaska.release
  for (auto call : collectCalls(M, "alaska.release")) {
    to_delete.insert(call);
    // IRBuilder<> b(call);  // insert after the call
    // b.CreateLifetimeEnd(call->getArgOperand(0));
  }

  // Lower alaska.derive
  for (auto call : collectCalls(M, "alaska.derive")) {
    // Release is just a marker for liveness. We don't need (or want) it to be in the
    // application at the end
    IRBuilder<> b(call);  // insert after the call
                          //
    auto base = call->getArgOperand(0);
    auto offset = dyn_cast<llvm::GetElementPtrInst>(call->getArgOperand(1));

    // std::unordered_map<Value *, APInt> varOffs;
    // APInt constOff;
    // if (collectOffsets(offset, DL, bitWidth, varOffs, constOff)) {
    //   alaska::println("offsets for: ", *offset);
    //   alaska::println("const: ", constOff);
    //   alaska::println(" vars:");
    //   for (auto &[k, v] : varOffs) {
    //     alaska::println("      ", v, " bytes from ", *k);
    //   }
    // } else {
    //   alaska::println("Nothing for ", *offset);
    // }


    std::vector<llvm::Value *> inds(offset->idx_begin(), offset->idx_end());
    auto gep = b.CreateGEP(offset->getSourceElementType(), base, inds, "", offset->isInBounds());
    call->replaceAllUsesWith(gep);

    to_delete.insert(call);
  }

  for (auto inst_to_delete : to_delete) {
    inst_to_delete->eraseFromParent();
  }

#ifdef ALASKA_VERIFY_PASS
  for (auto &F : M) {
    llvm::verifyFunction(F);
  }
#endif


  // errs() << M << "\n";
  return PreservedAnalyses::none();
}
