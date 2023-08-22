#include <alaska/Passes.h>
#include <alaska/Translations.h>
#include <alaska/Utils.h>
#include <llvm/IR/InstVisitor.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <set>
#include <llvm/IR/Verifier.h>

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

llvm::PreservedAnalyses AlaskaLowerPass::run(llvm::Module &M, llvm::ModuleAnalysisManager &AM) {
  std::set<llvm::Instruction *> to_delete;


	// Lower GC rooting.
  if (auto func = M.getFunction("alaska.safepoint_poll")) {
    auto pollFunc = M.getOrInsertFunction("alaska_safepoint", func->getFunctionType());
    for (auto call : collectCalls(M, "alaska.safepoint_poll")) {
      call->setCalledFunction(pollFunc);
    }
  }

  // Lower alaska.root
  for (auto *call : collectCalls(M, "alaska.root")) {
    // alaska::println(*call);
    call->replaceAllUsesWith(call->getArgOperand(0));
		// alaska::println("root:", *call->getArgOperand(0));
    to_delete.insert(call);
    if (auto *invoke = dyn_cast<llvm::InvokeInst>(call)) {
      auto *landing_pad = invoke->getLandingPadInst();
      to_delete.insert(landing_pad);
    }
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
    to_delete.insert(call);
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
