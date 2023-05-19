#include <alaska/Passes.h>
#include <alaska/Translations.h>
#include <alaska/Utils.h>
#include <llvm/IR/InstVisitor.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <set>

using namespace llvm;


std::vector<llvm::CallInst *> collectCalls(llvm::Module &M, const char *name) {
  std::vector<llvm::CallInst *> calls;

  if (auto func = M.getFunction(name)) {
    for (auto user : func->users()) {
      if (auto call = dyn_cast<CallInst>(user)) {
        calls.push_back(call);
      }
    }
  }


  return calls;
}

llvm::PreservedAnalyses AlaskaLowerPass::run(llvm::Module &M, llvm::ModuleAnalysisManager &AM) {
  // Lower alaska.root
  for (auto call : collectCalls(M, "alaska.root")) {
    call->replaceAllUsesWith(call->getArgOperand(0));
    call->eraseFromParent();
  }


  // Lower alaska.translate
  if (auto func = M.getFunction("alaska.translate")) {
    auto translateFunc = M.getOrInsertFunction("alaska_translate", func->getFunctionType());
    for (auto call : collectCalls(M, "alaska.translate")) {
      call->setCalledFunction(translateFunc);
    }
  }

  // Lower alaska.release
  for (auto call : collectCalls(M, "alaska.release")) {
    call->eraseFromParent();
  }

  // Lower alaska.derive
  for (auto call : collectCalls(M, "alaska.derive")) {
    // Release is just a marker for liveness. We don't need (or want) it to be in the
    // application at the end
    IRBuilder<> b(call);  // insert after the call
                          //
    auto base = call->getArgOperand(0);
    auto offset = dyn_cast<llvm::GetElementPtrInst>(call->getArgOperand(1));

    std::vector<llvm::Value *> inds(offset->idx_begin(), offset->idx_end());
    auto gep = b.CreateGEP(offset->getSourceElementType(), base, inds, "", offset->isInBounds());
    call->replaceAllUsesWith(gep);

    call->eraseFromParent();
  }


  // errs() << M << "\n";
  return PreservedAnalyses::none();
}
