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
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/Linker/Linker.h"

#include "noelle/core/DataFlow.hpp"
#include "noelle/core/MetadataManager.hpp"



static void run_on_function(Function &F) {
  if (F.empty()) return;
  auto section = F.getSection();
  // Skip functions which are annotated with `alaska_rt`
  if (section.startswith("$__ALASKA__")) {
    F.setSection("");
    return;
  }

#ifdef ALASKA_CONSERVATIVE
  alaska::PointerFlowGraph graph(F);
  alaska::insertConservativeTranslations(graph);
#else
  alaska::LockForestTransformation fftx(F);
  fftx.apply();
#endif
}

using namespace llvm;

// For now, alaska is a function pass, and does not perform whole program analysis
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
      run_on_function(F);
    }
    return true;
  }
  void getAnalysisUsage(AnalysisUsage &AU) const override {}
};

static RegisterPass<AlaskaPass> X("alaska", "Alaska Translation", false, false);
char AlaskaPass::ID = 0;




// #include "clang/Frontend/FrontendPluginRegistry.h"
// #include "clang/AST/AST.h"
// #include "clang/AST/ASTConsumer.h"
// #include "clang/AST/RecursiveASTVisitor.h"
// #include "clang/Frontend/CompilerInstance.h"
// #include "clang/Sema/Sema.h"
// #include "llvm/Support/raw_ostream.h"
// using namespace clang;
// // Define a pragma handler for #pragma example_pragma
// class ExamplePragmaHandler : public PragmaHandler {
// public:
//   ExamplePragmaHandler() : PragmaHandler("alaska") { }
//   void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
//                     Token &PragmaTok) override {
// 		printf("example pragma!\n");
//   }
// };
// static PragmaHandlerRegistry::Add<ExamplePragmaHandler> Y("alaska","example pragma description");
