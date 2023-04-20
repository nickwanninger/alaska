// Alaska includes
#include <Graph.h>
#include <Utils.h>
#include <Locks.h>
#include <ct/Pass.hpp>
#include <Passes.h>

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



template <typename T>
auto adapt(T &&fp) {
  FunctionPassManager FPM;
  FPM.addPass(std::move(fp));
  return createModuleToFunctionPassAdaptor(std::move(FPM));
}

// Register the alaska passes with the new pass manager
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "Alaska", LLVM_VERSION_STRING, [](PassBuilder &PB) {
            PB.registerOptimizerLastEPCallback([](ModulePassManager &MPM, OptimizationLevel optLevel) {
              // MPM.addPass(AlaskaLinkLibraryPass(ALASKA_INSTALL_PREFIX "/lib/alaska_compat.bc"));

              if (getenv("ALASKA_COMPILER_BASELINE") == NULL) {
                MPM.addPass(AlaskaNormalizePass());
                if (!alaska::bootstrapping()) {
                  // run replacement on non-bootstrapped code
                  MPM.addPass(AlaskaReplacementPass());
                }

                MPM.addPass(AlaskaTranslatePass());
#ifdef ALASKA_COMPILER_TIMING
                if (!alaska::bootstrapping()) MPM.addPass(CompilerTimingPass());
#endif
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

                if (alaska::bootstrapping()) {
                  // Use the bootstrap bitcode if we are bootstrapping
                  MPM.addPass(AlaskaLinkLibraryPass(ALASKA_INSTALL_PREFIX "/lib/alaska_bootstrap.bc"));
                } else {
                  // Link the library otherwise (just runtime/src/translate.c)
                  MPM.addPass(AlaskaLinkLibraryPass(ALASKA_INSTALL_PREFIX "/lib/alaska_translate.bc"));
                }
              }


              return true;
            });
          }};
}
