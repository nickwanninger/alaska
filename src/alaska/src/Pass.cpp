// #include "analysis/Grpah.h"


#include <Graph.h>
#include <cassert>
#include <unordered_set>

#include "llvm/IR/Operator.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/PostDominators.h"

using namespace llvm;

namespace {
  struct AlaskaPass : public ModulePass {
    static char ID;
    llvm::Type *int64Type;

    AlaskaPass() : ModulePass(ID) {}

    bool doInitialization(Module &M) override { return false; }

    bool runOnModule(Module &M) override {
      if (M.getNamedMetadata("alaska") != nullptr) {
        return false;
      }
      M.getOrInsertNamedMetadata("alaska");
      // LLVMContext &ctx = M.getContext();
      // int64Type = Type::getInt64Ty(ctx);
      // auto ptrType = PointerType::get(ctx, 0);

      // auto pinFunctionType = FunctionType::get(ptrType, {ptrType}, false);
      // auto pinFunction = M.getOrInsertFunction("alaska_pin", pinFunctionType).getCallee();

      // auto unpinFunctionType = FunctionType::get(Type::getVoidTy(ctx), {ptrType}, false);
      // auto unpinFunction = M.getOrInsertFunction("alaska_unpin", unpinFunctionType).getCallee();
      // (void)pinFunction;
      // (void)unpinFunction;

      for (auto &F : M) {
        if (F.empty()) continue;
        auto section = F.getSection();
        // Skip functions which are annotated with `alaska_rt`
        if (section.startswith("$__ALASKA__")) {
          F.setSection("");
          continue;
        }

				auto *DT = &getAnalysis<DominatorTreeWrapperPass>(F).getDomTree();
				auto *PDT = &getAnalysis<PostDominatorTreeWrapperPass>(F).getPostDomTree();
        alaska::PinGraph graph(F);
				auto nodes = graph.get_nodes();

        graph.dump_dot(*DT, *PDT);
      }

      return false;
    }


    void getAnalysisUsage(AnalysisUsage &AU) const override {
      // our pass requires Alias Analysis
      AU.addRequired<AAResultsWrapperPass>();

      // some loop stuffs
      AU.addRequired<AssumptionCacheTracker>();
      AU.addRequired<DominatorTreeWrapperPass>();
			AU.addRequired<PostDominatorTreeWrapperPass>();
      AU.addRequired<LoopInfoWrapperPass>();
      AU.addRequired<ScalarEvolutionWrapperPass>();
      // AU.addRequired<TargetTransformInfoWrapperPass>();
    }
  };  // namespace
      //
  static RegisterPass<AlaskaPass> X("alaska", "Alaska Pinning", false /* Only looks at CFG */, false /* Analysis Pass */);

  char AlaskaPass::ID = 0;

  // static AlaskaPass *_PassMaker = NULL;
  // static RegisterStandardPasses _RegPass1(
  //     PassManagerBuilder::EP_OptimizerLast, [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
  //       if (!_PassMaker) {
  //         PM.add(_PassMaker = new AlaskaPass());
  //       }
  //     });  // ** for -Ox
  // static RegisterStandardPasses _RegPass2(
  //     PassManagerBuilder::EP_EnabledOnOptLevel0, [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
  //       if (!_PassMaker) {
  //         PM.add(_PassMaker = new AlaskaPass());
  //       }
  //     });

}  // namespace
