// Alaska includes
#include <Graph.h>
#include <Utils.h>
#include <Locks.h>
#include <ct/Pass.hpp>
#include <Passes.h>

// C++ includes
#include <cassert>
#include <set>

// LLVM includes
#include "llvm/IR/Constants.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/Pass.h"
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
#include "llvm/Transforms/Utils/LowerInvoke.h"
#include "llvm/Transforms/Scalar/DCE.h"
#include "llvm/Transforms/IPO/Inliner.h"
#include "llvm/Transforms/IPO/GlobalDCE.h"
#include "llvm/Transforms/IPO/AlwaysInliner.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Transforms/Utils/EscapeEnumerator.h"
#include "llvm/Transforms/Utils/Mem2Reg.h"

#include "noelle/core/DataFlow.hpp"
#include "noelle/core/MetadataManager.hpp"

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
                // MPM.addPass(CompilerTimingPass());
                MPM.addPass(adapt(PromotePass()));

#ifdef ALASKA_ESCAPE_PASS
                if (!alaska::bootstrapping()) {
                  MPM.addPass(AlaskaEscapePass());
                }
#endif
#ifdef ALASKA_DUMP_LOCKS
                MPM.addPass(LockPrinterPass());
#endif

                // MPM.addPass(LockRemoverPass());
                // MPM.addPass(RedundantArgumentLockElisionPass());
                MPM.addPass(LockTrackerPass());

                if (alaska::bootstrapping()) {
                  // Use the bootstrap bitcode if we are bootstrapping
                  MPM.addPass(AlaskaLinkLibraryPass(ALASKA_INSTALL_PREFIX "/lib/alaska_bootstrap.bc"));
                } else {
                  // Link the library otherwise (just runtime/src/lock.c)
                  MPM.addPass(AlaskaLinkLibraryPass(ALASKA_INSTALL_PREFIX "/lib/alaska_lock.bc"));
                }
              }


              return true;
            });
          }};
}
