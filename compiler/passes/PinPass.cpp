// #include "analysis/Grpah.h"


#include <Graph.h>
#include <cassert>
#include <unordered_set>

#include "llvm/IR/Operator.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/PostDominators.h"

#include "noelle/core/MetadataManager.hpp"


using namespace llvm;

namespace {
  struct AlaskaPass : public ModulePass {
    static char ID;
    llvm::Type *int64Type;

    AlaskaPass() : ModulePass(ID) {}

    bool doInitialization(Module &M) override { return false; }

    bool runOnModule(Module &M) override {
      llvm::noelle::MetadataManager mdm(M);

      if (mdm.doesHaveMetadata("alaska")) {
        return false;
      }

      mdm.addMetadata("alaska", "did run");
      LLVMContext &ctx = M.getContext();
      int64Type = Type::getInt64Ty(ctx);
      auto ptrType = PointerType::get(ctx, 0);

      auto pinFunctionType = FunctionType::get(ptrType, {ptrType}, false);
      auto pinFunction = M.getOrInsertFunction("alaska_pin", pinFunctionType).getCallee();

      auto unpinFunctionType = FunctionType::get(Type::getVoidTy(ctx), {ptrType}, false);
      auto unpinFunction = M.getOrInsertFunction("alaska_unpin", unpinFunctionType).getCallee();
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

        // auto *DT = &getAnalysis<DominatorTreeWrapperPass>(F).getDomTree();
        // auto *PDT = &getAnalysis<PostDominatorTreeWrapperPass>(F).getPostDomTree();
        alaska::PinGraph graph(F);
        auto nodes = graph.get_nodes();

        for (auto node : nodes) {
          if (node->type != alaska::Sink) continue;
          auto I = dyn_cast<Instruction>(node->value);

          llvm::IRBuilder<> b(I);

					llvm::Value *ptr = NULL;

          //
          if (auto *load = dyn_cast<LoadInst>(I)) {
						ptr = load->getPointerOperand();
						auto pinned = b.CreateCall(pinFunctionType, pinFunction, {ptr});
						load->setOperand(0, pinned);
						b.SetInsertPoint(I->getNextNode());
						b.CreateCall(unpinFunctionType, unpinFunction, {ptr});
          }

          //
          if (auto *store = dyn_cast<StoreInst>(I)) {
						ptr = store->getPointerOperand();
						auto pinned = b.CreateCall(pinFunctionType, pinFunction, {ptr});
						store->setOperand(1, pinned);
						b.SetInsertPoint(I->getNextNode());
						b.CreateCall(unpinFunctionType, unpinFunction, {ptr});
          }


        }

        // for (auto node : nodes) {
        //   if (auto I = dyn_cast<Instruction>(node->value)) {
        //     alaska::println(*I);
        //     if (!mdm.doesHaveMetadata(I, "alaska")) {
        //       mdm.addMetadata(I, "alaska", "it's a handle!");
        //     }
        //   }
        // }

        // graph.dump_dot(*DT, *PDT);
      }

      // alaska::println(M);

      return true;
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
