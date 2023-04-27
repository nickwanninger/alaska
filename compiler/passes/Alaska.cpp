// Alaska includes
#include <alaska/PointerFlowGraph.h>
#include <alaska/Utils.h>
#include <alaska/Translations.h>
#include <ct/Pass.hpp>
#include <alaska/Passes.h>

// LLVM includes
#include <llvm/Pass.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>
#include <llvm/Transforms/Utils/Mem2Reg.h>

// Noelle Includes
#include <noelle/core/DataFlow.hpp>
#include <noelle/core/MetadataManager.hpp>

// C++ includes
#include <cassert>
#include <set>
#include <optional>



#include <noelle/core/DGBase.hpp>


static void replace_function(Module &M, std::string original_name, std::string new_name = "") {
  if (new_name == "") {
    new_name = "alaska_wrapped_" + original_name;
  }
  auto oldFunction = M.getFunction(original_name);
  if (oldFunction) {
    auto newFunction = M.getOrInsertFunction(new_name, oldFunction->getType()).getCallee();
    oldFunction->replaceAllUsesWith(newFunction);
    oldFunction->eraseFromParent();
  }
}

class PrefetchPass : public llvm::PassInfoMixin<PrefetchPass> {
 public:
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &AM) {
    replace_function(M, "alaska_translate", "alaska_prefetch_translate");
    replace_function(M, "alaska_release", "alaska_prefetch_release");
    return PreservedAnalyses::none();
  }
};



template <typename T>
auto adapt(T &&fp) {
  FunctionPassManager FPM;
  FPM.addPass(std::move(fp));
  return createModuleToFunctionPassAdaptor(std::move(FPM));
}

// Register the alaska passes with the new pass manager
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {
      LLVM_PLUGIN_API_VERSION, "Alaska", LLVM_VERSION_STRING, [](PassBuilder &PB) {
        PB.registerOptimizerLastEPCallback([](ModulePassManager &MPM, OptimizationLevel optLevel) {
          // MPM.addPass(AlaskaLinkLibraryPass(ALASKA_INSTALL_PREFIX "/lib/alaska_compat.bc"));

          if (getenv("ALASKA_ONLY_PREFETCH") != NULL) {
            MPM.addPass(AlaskaTranslatePass());
            MPM.addPass(PrefetchPass());
            MPM.addPass(AlaskaLinkLibraryPass(ALASKA_INSTALL_PREFIX "/lib/alaska_prefetch.bc"));
            return true;
          }

          if (getenv("ALASKA_COMPILER_BASELINE") == NULL) {
            MPM.addPass(AlaskaNormalizePass());
            if (!alaska::bootstrapping()) {
              // run replacement on non-bootstrapped code
              MPM.addPass(AlaskaReplacementPass());
            }

#ifdef ALASKA_COMPILER_TIMING
            if (!alaska::bootstrapping()) MPM.addPass(CompilerTimingPass());
#endif


            MPM.addPass(AlaskaTranslatePass());
            MPM.addPass(adapt(PromotePass()));

#ifdef ALASKA_ESCAPE_PASS
            if (!alaska::bootstrapping()) {
              MPM.addPass(AlaskaEscapePass());
            }
#endif
#ifdef ALASKA_DUMP_LOCKS
            MPM.addPass(LockPrinterPass());
#endif
            // MPM.addPass(RedundantArgumentLockElisionPass());
            if (!alaska::bootstrapping()) MPM.addPass(LockTrackerPass());

            // if (alaska::bootstrapping()) {
            //   // Use the bootstrap bitcode if we are bootstrapping
            //   MPM.addPass(AlaskaLinkLibraryPass(ALASKA_INSTALL_PREFIX
            //   "/lib/alaska_bootstrap.bc"));
            // } else {
            //   // Link the library otherwise (just runtime/src/translate.c)
            //   MPM.addPass(AlaskaLinkLibraryPass(ALASKA_INSTALL_PREFIX
            //   "/lib/alaska_translate.bc"));
            // }
          }


          return true;
        });
      }};
}
