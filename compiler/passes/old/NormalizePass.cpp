#include "llvm/Pass.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/DerivedUser.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Mangler.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Operator.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/LoopUtils.h"

#include <unordered_set>

using namespace llvm;

// Compute parent and type for instructions before inserting them into the trace.
class NormalizeVisitor : public llvm::InstVisitor<NormalizeVisitor> {
 public:
  llvm::Value *handleOperator(llvm::Operator *op, llvm::Instruction *inst) {
    IRBuilder<> b(inst);

    if (auto bitCastOp = dyn_cast<BitCastOperator>(op)) {
      return new llvm::BitCastInst(bitCastOp->getOperand(0), bitCastOp->getDestTy(), "", inst);
    }

    if (auto gep = dyn_cast<GEPOperator>(op)) {
      // errs() << "found gep operator\n";
      std::vector<llvm::Value *> inds(gep->idx_begin(), gep->idx_end());
      if (gep->isInBounds()) {
        gep->hasIndices();
        return llvm::GetElementPtrInst::CreateInBounds(
            gep->getSourceElementType(), gep->getPointerOperand(), inds, "", inst);
      } else {
        return llvm::GetElementPtrInst::Create(gep->getSourceElementType(), gep->getPointerOperand(), inds, "", inst);
      }
    }

    return nullptr;
  }

  void visitStoreInst(llvm::StoreInst &I) {
    if (auto opInst = dyn_cast<llvm::Instruction>(I.getPointerOperand())) {
      return;
    }
    if (auto op = dyn_cast<llvm::Operator>(I.getPointerOperand())) {
      auto n = handleOperator(op, &I);
      if (n) I.setOperand(1, n);
    }
  }

  void visitLoadInst(llvm::LoadInst &I) {
    if (auto opInst = dyn_cast<llvm::Instruction>(I.getPointerOperand())) {
      return;
    }
    if (auto op = dyn_cast<llvm::Operator>(I.getPointerOperand())) {
      auto n = handleOperator(op, &I);
      if (n) I.setOperand(0, n);
    }
  }
};

namespace {
  struct AlaskaPass : public ModulePass {
    static char ID;
    AlaskaPass() : ModulePass(ID) {}


    // Normalize select statements that have a pointer type
    void normalizeSelects(Function &F) {
      std::unordered_set<llvm::SelectInst *> selects;

      for (auto &BB : F) {
        for (auto &I : BB) {
          if (auto sinst = dyn_cast<SelectInst>(&I)) {
            if (sinst->getType()->isPointerTy()) {
              selects.insert(sinst);
            }
          }
        }
      }
      if (selects.size() == 0) return;

      for (auto *sel : selects) {
        Instruction *thenTerm, *elseTerm;
        SplitBlockAndInsertIfThenElse(sel->getCondition(), sel->getNextNode(), &thenTerm, &elseTerm);

        auto termBB = dyn_cast<BranchInst>(thenTerm)->getSuccessor(0);

        IRBuilder<> b(termBB->getFirstNonPHI());
        auto phi = b.CreatePHI(sel->getType(), 2);
        phi->addIncoming(sel->getTrueValue(), thenTerm->getParent());
        phi->addIncoming(sel->getFalseValue(), elseTerm->getParent());
        sel->replaceAllUsesWith(phi);
        sel->eraseFromParent();
      }
    }

    bool doInitialization(Module &M) override { return false; }

    bool runOnModule(Module &M) override {
      NormalizeVisitor v;
      std::vector<llvm::Instruction *> insts;

      for (auto &F : M) {
        if (F.empty()) continue;
        for (auto &BB : F) {
          for (auto &I : BB) {
            insts.push_back(&I);
          }
        }
      }

      for (auto *I : insts) {
        v.visit(I);
      }


      for (auto &F : M)
        normalizeSelects(F);

      return true;
    }
  };

  static RegisterPass<AlaskaPass> X(
      "alaska-norm", "Alaska Normalize", false /* Only looks at CFG */, false /* Analysis Pass */);

  char AlaskaPass::ID = 0;
  // static RegisterPass<AlaskaPass> X("Alaska", "Handle based memory with Alaska");

  static AlaskaPass *_PassMaker = NULL;
  static RegisterStandardPasses _RegPass1(
      PassManagerBuilder::EP_OptimizerLast, [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
        if (!_PassMaker) {
          PM.add(_PassMaker = new AlaskaPass());
        }
      });  // ** for -Ox
  static RegisterStandardPasses _RegPass2(
      PassManagerBuilder::EP_EnabledOnOptLevel0, [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
        if (!_PassMaker) {
          PM.add(_PassMaker = new AlaskaPass());
        }
      });  // ** for -O0
}  // namespace