#include <alaska/Passes.h>
#include <alaska/Translations.h>
#include <alaska/Utils.h>
#include <llvm/IR/InstVisitor.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <set>

using namespace llvm;


// static bool callsSelf(llvm::Function &F) {
//   for (auto user : F.users()) {
//     if (auto call = dyn_cast<CallInst>(user)) {
//       if (call->getFunction() == &F) {
//         return true;
//       }
//     }
//   }
//   return false;
// }
//
//
//
// static bool callbackDevirtualize(llvm::Function &F) {
//   std::set<llvm::Argument *> calledArguments;
//
//   // Are any of the arguments called as a function?
//   for (auto &A : F.args()) {
//     for (auto user : A.users()) {
//       if (auto call = dyn_cast<CallInst>(user)) {
//         if (call->getCalledOperand() == &A) {
//           calledArguments.insert(&A);
//         }
//       }
//     }
//   }
//
//   if (calledArguments.size() == 0) {
//     return false;
//   }
//
//   if (calledArguments.size() != 1) {
//     alaska::println(
//         "don't know what to do with multiple argument devirtualization in ", F.getName());
//     return false;
//   }
//
//   alaska::println("might be interested in devirtualizing ", F.getName());
//   if (callsSelf(F)) {
//     alaska::println("It is self recursive");
//   }
//   for (auto *arg : calledArguments) {
//     alaska::println(" - ", *arg);
//     for (auto user : arg->users()) {
//       if (auto call = dyn_cast<CallInst>(user)) {
//         if (call->getCalledOperand() == arg) {
//           alaska::println("   ", *call);
//         }
//       }
//     }
//   }
//
//
//   alaska::println("Callers:");
//   for (auto user : F.users()) {
//     if (auto call = dyn_cast<CallInst>(user)) {
// 			if (call->getFunction() != &F && call->getCalledOperand() == &F) {
// 				alaska::println(*call);
// 			}
//     }
//   }
//
//   return true;
// }


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
        return llvm::GetElementPtrInst::Create(
            gep->getSourceElementType(), gep->getPointerOperand(), inds, "", inst);
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

	// // Attempt to inline callbacks. The main drive for this is to reduce the overhead of calling
 //  for (auto &F : M) {
 //    callbackDevirtualize(F);
 //  }
  return PreservedAnalyses::none();
}
