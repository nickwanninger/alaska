// Alaska includes
#include <alaska/PointerFlowGraph.h>
#include <alaska/Utils.h>
#include <alaska/Translations.h>
#include <ct/Pass.hpp>
#include <alaska/Passes.h>
#include <alaska/PlaceSafepoints.h>  // Stolen from LLVM

// LLVM includes
#include <llvm/Pass.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>
#include <llvm/Transforms/Utils/Mem2Reg.h>
#include <llvm/Transforms/Utils/LowerInvoke.h>
#include <llvm/Transforms/Utils/LowerSwitch.h>
#include "llvm/Transforms/Scalar/DCE.h"
#include "llvm/Transforms/Scalar/ADCE.h"
#include "llvm/Transforms/Scalar/SCCP.h"
#include "llvm/IR/PassTimingInfo.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"
// #include "llvm/Transforms/Scalar/PlaceSafepoints.h"
#include "llvm/Transforms/Scalar/RewriteStatepointsForGC.h"

// Noelle Includes
#include <noelle/core/DataFlow.hpp>
#include <noelle/core/MetadataManager.hpp>

// C++ includes
#include <cassert>
#include <set>
#include <optional>

// #include "../../runtime/include/alaska/utils.h"


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



class PrintPassThing : public llvm::PassInfoMixin<PrintPassThing> {
 public:
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &AM) {
    for (auto &F : M) {
      errs() << F << "\n";
    }
    return PreservedAnalyses::all();
  }
};


static bool print_progress = true;
class ProgressPass : public llvm::PassInfoMixin<ProgressPass> {
 public:
  static double progress_start;

  const char *message = NULL;
  ProgressPass(const char *message)
      : message(message) {
  }
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &AM) {
    double now = alaska::time_ms();
    if (progress_start == 0) {
      progress_start = now;
    }

    if (print_progress) {
      printf("\e[32m[progress]\e[0m %-20s %10.4fms\n", message, now - progress_start);
    }
    progress_start = now;
    return PreservedAnalyses::all();
  }
};
double ProgressPass::progress_start = 0.0;


class PrintyPass : public llvm::PassInfoMixin<PrintyPass> {
 public:
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &AM) {
    for (auto &F : M) {
      if (F.getName() == "main") alaska::println(F);
    }
    return PreservedAnalyses::all();
  }
};


class TranslationInlinePass : public llvm::PassInfoMixin<TranslationInlinePass> {
 public:
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &AM) {
    std::vector<llvm::CallInst *> toInline;
    for (auto &F : M) {
      if (F.empty()) continue;
      if (F.getName().startswith("alaska_")) {
        for (auto user : F.users()) {
          if (auto call = dyn_cast<CallInst>(user)) {
            if (call->getCalledFunction() != &F) continue;
            toInline.push_back(call);
          }
        }
      }
    }

    for (auto *call : toInline) {
      llvm::InlineFunctionInfo IFI;
      llvm::InlineFunction(*call, IFI);
    }
    return PreservedAnalyses::none();
  }
};




template <typename T>
auto adapt(T &&fp) {
  FunctionPassManager FPM;
  FPM.addPass(std::move(fp));
  return createModuleToFunctionPassAdaptor(std::move(FPM));
}

void populateMPM(ModulePassManager &MPM) {
  bool baseline = getenv("ALASKA_COMPILER_BASELINE") != NULL;

  if (baseline) print_progress = false;


  // Only link the stub in non-baseline
  if (!baseline) {
    MPM.addPass(AlaskaLinkLibraryPass(ALASKA_INSTALL_PREFIX "/lib/alaska_stub.bc"));
    MPM.addPass(ProgressPass("Link Stub"));
  }



  MPM.addPass(adapt(DCEPass()));
  MPM.addPass(adapt(DCEPass()));
  MPM.addPass(adapt(ADCEPass()));
  MPM.addPass(ProgressPass("DCE"));

  // Run a normalization pass regardless of the environment configuration
  MPM.addPass(AlaskaNormalizePass());
  MPM.addPass(ProgressPass("Normalize"));


  // Simplify the cfg
  MPM.addPass(adapt(SimplifyCFGPass()));

  if (!baseline) {
    MPM.addPass(AlaskaReplacementPass());
    MPM.addPass(ProgressPass("Replacement"));

    MPM.addPass(AlaskaTranslatePass());
    MPM.addPass(ProgressPass("Translate"));

#ifdef ALASKA_ESCAPE_PASS
    MPM.addPass(AlaskaEscapePass());
    MPM.addPass(ProgressPass("Escape"));
#endif

#ifdef ALASKA_DUMP_TRANSLATIONS
    MPM.addPass(LockPrinterPass());
    MPM.addPass(ProgressPass("Lock Printing"));
#endif

    // MPM.addPass(adapt(PlaceSafepointsPass()));
    // MPM.addPass(ProgressPass("Safepoint Placement"));

    if (!alaska::bootstrapping()) {
      MPM.addPass(LockInsertionPass());
      MPM.addPass(ProgressPass("Lock Insertion"));
    }

#ifdef ALASKA_INLINE_TRANSLATION
    MPM.addPass(AlaskaLinkLibraryPass(ALASKA_INSTALL_PREFIX "/lib/alaska_translate.bc"));
    MPM.addPass(ProgressPass("Link runtime"));
#endif

    MPM.addPass(AlaskaLowerPass());
    MPM.addPass(ProgressPass("Lowering"));

#ifdef ALASKA_INLINE_TRANSLATION
    // Force inlines of alaska runtime functions
    MPM.addPass(TranslationInlinePass());
    MPM.addPass(ProgressPass("Inline runtime"));
#endif
  }
  MPM.addPass(RewriteStatepointsForGC());
  // MPM.addPass(PrintyPass());


  MPM.addPass(adapt(DCEPass()));
  MPM.addPass(adapt(DCEPass()));
  MPM.addPass(adapt(ADCEPass()));
  MPM.addPass(ProgressPass("DCE"));
}

// llvm::PassPluginLibraryInfo getPluginInfo() {
//   return {LLVM_PLUGIN_API_VERSION, "alaska", LLVM_VERSION_STRING, [](PassBuilder &PB) {
//             PB.registerPipelineParsingCallback(
//                 [](StringRef Name, ModulePassManager &MPM,
//                 ArrayRef<PassBuilder::PipelineElement>) {
//                   alaska::println("what up ", Name);
//                   populateMPM(MPM);
//                   // MPM.addPass(CompilerTimingPass());
//                   return true;
//                 });
//           }};
// }

// extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
//   return getPluginInfo();
// }

// Register the alaska passes with the new pass manager
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "Alaska", LLVM_VERSION_STRING, [](PassBuilder &PB) {
            PB.registerOptimizerLastEPCallback(
                [](ModulePassManager &MPM, OptimizationLevel optLevel) {
                  populateMPM(MPM);
                });
          }};
}
