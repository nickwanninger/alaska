// #include "analysis/Grpah.h"


// Alaska includes
#include <Graph.h>
#include <Utils.h>

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

      // to compute average user ratios
      std::vector<long> source_users;

      for (auto &F : M) {
        if (F.empty()) continue;
        auto section = F.getSection();
        // Skip functions which are annotated with `alaska_rt`
        if (section.startswith("$__ALASKA__")) {
          F.setSection("");
          continue;
        }

        alaska::PointerFlowGraph graph(F);
        auto nodes = graph.get_nodes();

#ifdef ALASKA_DUMP_GRAPH
        llvm::DominatorTree DT(F);
        llvm::PostDominatorTree PDT(F);
        graph.dump_dot(DT, PDT);
#endif

#ifdef ALASKA_CONSERVATIVE
        alaska::insertConservativeTranslations(graph);
#else
        alaska::insertNaiveFlowBasedTranslations(graph);
#endif
      }

      return true;
    }


    void getAnalysisUsage(AnalysisUsage &AU) const override {
      // We need dominator and postdominator trees
      AU.addRequired<DominatorTreeWrapperPass>();
      AU.addRequired<PostDominatorTreeWrapperPass>();
    }
  };

  static RegisterPass<AlaskaPass> X("alaska", "Alaska Translation", false, false);
  char AlaskaPass::ID = 0;

}  // namespace
