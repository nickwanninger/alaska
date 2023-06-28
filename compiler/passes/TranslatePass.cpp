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

  for (auto &F : M) {
    if (F.empty()) continue;
    auto section = F.getSection();
    // Skip functions which are annotated with `alaska_rt`
    if (section.startswith("$__ALASKA__")) {
      F.setSection("");
      continue;
    }

		auto start = alaska::timestamp();
#ifdef ALASKA_HOIST_TRANSLATIONS
    alaska::insertHoistedTranslations(F);
#else
    alaska::insertConservativeTranslations(F);
#endif
		auto end = alaska::timestamp();
		(void)(end - start);
		// printf("%s,%f\n", F.getName().data(), (end - start) / 1000.0 / 100.0);
		// exit(-1);
  }


  return PreservedAnalyses::none();
}
