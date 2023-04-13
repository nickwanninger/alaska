#include <Passes.h>
#include <Locks.h>
#include <Utils.h>
#include <llvm/IR/InstVisitor.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

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

// Normalize select statements that have a pointer type
void normalizeSelects(Function &F) {
  std::set<llvm::SelectInst *> selects;

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

llvm::PreservedAnalyses AlaskaNormalizePass::run(llvm::Module &M, llvm::ModuleAnalysisManager &AM) {
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

  for (auto *I : insts)
    v.visit(I);


  for (auto &F : M)
    normalizeSelects(F);
  return PreservedAnalyses::none();
}