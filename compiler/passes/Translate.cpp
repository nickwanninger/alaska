#include <Passes.h>
#include <Locks.h>
#include <Utils.h>
#include <llvm/IR/InstVisitor.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <noelle/core/MetadataManager.hpp>

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

    // alaska::println("running translate on ", F.getName());
#ifdef ALASKA_HOIST_LOCKS
    alaska::insertHoistedLocks(F);
#else
    alaska::insertConservativeLocks(F);
#endif
  }


  return PreservedAnalyses::none();
}