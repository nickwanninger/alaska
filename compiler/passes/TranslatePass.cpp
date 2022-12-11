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

        alaska::PointerFlowGraph graph(F);
        auto nodes = graph.get_nodes();

				// llvm::noelle::DataFlowAnalysis dfa;
				// auto df = dfa.runReachableAnalysis(&F);
				// for (auto &BB : F) {
				// 	for (auto &I : BB) {
				// 		alaska::println(I);
				// 		auto &OUT = df->OUT(&I);
				// 		for (auto out : OUT) {
				// 			alaska::println("   ", *out);
				// 		}
				//
				// 	}
				// }
				// delete df;

#ifdef ALASKA_DUMP_GRAPH
        llvm::DominatorTree DT(F);
        llvm::PostDominatorTree PDT(F);
        graph.dump_dot(DT, PDT);
#endif

#ifdef ALASKA_CONSERVATIVE
        alaska::insertConservativeTranslations(graph);
#else
        // alaska::insertNaiveFlowBasedTranslations(graph);
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
