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
#include "llvm/IR/Verifier.h"

#include <memory>
#include <optional>
#include <utility>
using namespace llvm;


static bool no_strict_alias(void) {
  static bool set = getenv("ALASKA_NO_STRICT_ALIAS") != NULL;
  return set;
}

PreservedAnalyses AlaskaTranslatePass::run(Module &M, ModuleAnalysisManager &AM) {
  llvm::noelle::MetadataManager mdm(M);
  if (mdm.doesHaveMetadata("alaska")) {
    alaska::println("Alaska has already run on this module!\n");
    return PreservedAnalyses::all();
  }

  mdm.addMetadata("alaska", "did run");


  bool hoist = this->hoist;
  if (no_strict_alias()) {
    errs() << "WARNING: No strict alias is on. Disabling hoisting!\n";
    hoist = false;
  }


  for (auto &F : M) {
    if (F.empty()) continue;
    auto section = F.getSection();
    // Skip functions which are annotated with `alaska_rt`
    if (section.startswith("$__ALASKA__")) {
      F.setSection("");
      continue;
    }


    auto start = alaska::timestamp();
    if (hoist) {
      alaska::insertHoistedTranslations(F);
    } else {
      alaska::println("Not hoisting in ", F.getName());
      alaska::insertConservativeTranslations(F);
    }
    auto end = alaska::timestamp();
    (void)(end - start);



    if (verifyFunction(F, &errs())) {
      errs() << "Function verification failed!\n";
      errs() << F.getName() << "\n";
      auto l = alaska::extractTranslations(F);
      if (l.size() > 0) {
        alaska::printTranslationDot(F, l);
      }
      // errs() << F << "\n";
      exit(EXIT_FAILURE);
    }




    // printf("%s,%f\n", F.getName().data(), (end - start) / 1000.0 / 100.0);
    // exit(-1);
  }

  return PreservedAnalyses::none();
}
