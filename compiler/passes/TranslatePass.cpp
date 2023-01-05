// Alaska includes
#include <LockForest.h>
#include <Graph.h>
#include <Utils.h>
#include <LockForestTransformation.h>

// C++ includes
#include <cassert>
#include <unordered_set>

// LLVM includes
#include "llvm/IR/Operator.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Support/CommandLine.h"


#include "noelle/core/DataFlow.hpp"
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

      if (mdm.doesHaveMetadata("alaska")) return false;
      mdm.addMetadata("alaska", "did run");


      for (auto &F : M) {
        if (F.empty()) continue;
        auto section = F.getSection();
        // Skip functions which are annotated with `alaska_rt`
        if (section.startswith("$__ALASKA__")) {
          F.setSection("");
          continue;
        }

#ifdef ALASKA_CONSERVATIVE
        alaska::PointerFlowGraph graph(F);
        alaska::insertConservativeTranslations(graph);
#else
        alaska::LockForestTransformation fftx(F);
        fftx.apply();
#endif
      }

      return true;
    }


    void getAnalysisUsage(AnalysisUsage &AU) const override {
      // We need dominator and postdominator trees
      AU.addRequired<LoopInfoWrapperPass>();
      AU.addRequired<DominatorTreeWrapperPass>();
      AU.addRequired<PostDominatorTreeWrapperPass>();
    }
  };

  static RegisterPass<AlaskaPass> X("alaska", "Alaska Translation", false, false);
  char AlaskaPass::ID = 0;

}  // namespace
