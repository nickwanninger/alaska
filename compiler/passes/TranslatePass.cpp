#include <alaska/Passes.h>
#include <alaska/Translations.h>
#include <alaska/Utils.h>
#include <llvm/IR/InstVisitor.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <noelle/core/MetadataManager.hpp>

// #include <noelle/core/PDG.hpp>
// #include <noelle/core/PDGPrinter.hpp>
// #include "llvm/Analysis/BasicAliasAnalysis.h"
// #include "noelle/core/PDGAnalysis.hpp"

#include "llvm/IR/LegacyPassManager.h"

#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/ScopedNoAliasAA.h"
#include "llvm/Analysis/TypeBasedAliasAnalysis.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include <memory>
#include <optional>
#include <utility>
using namespace llvm;

PreservedAnalyses AlaskaTranslatePass::run(Module &M, ModuleAnalysisManager &AM) {
  llvm::noelle::MetadataManager mdm(M);
  if (mdm.doesHaveMetadata("alaska")) {
    alaska::println("Alaska has already run on this module!\n");
    return PreservedAnalyses::all();
  }

  mdm.addMetadata("alaska", "did run");

  // auto *PM = new llvm::legacy::PassManager();
  // noelle::PDGAnalysis pdga;
  // PM->add(new llvm::BasicAAWrapperPass());
  // PM->add(new llvm::ScopedNoAliasAAWrapperPass());
  // PM->add(new llvm::TypeBasedAAWrapperPass());
  // PM->add(&pdga);
  // PM->run(M);
  // auto *pdg = pdga.getPDG();
  //
  // void printPDG(Module &module, CallGraph &callGraph, PDG *graph, std::function<LoopInfo &(Function *f)> getLoopInfo);
  // PDGPrinter::writeGraph<PDG, Value>("pdg-full.dot", pdg);

  for (auto &F : M) {
    if (F.empty()) continue;
    auto section = F.getSection();
    // Skip functions which are annotated with `alaska_rt`
    if (section.startswith("$__ALASKA__")) {
      F.setSection("");
      continue;
    }

#ifdef ALASKA_HOIST_TRANSLATIONS
    alaska::insertHoistedTranslations(F);
#else
    alaska::insertConservativeTranslations(F);
#endif
  }


  return PreservedAnalyses::none();
}
