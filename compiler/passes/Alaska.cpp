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


class TranslationInlinePass : public llvm::PassInfoMixin<TranslationInlinePass> {
 public:
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &AM) {
    for (auto &F : M) {
      if (F.empty()) continue;
      if (F.getName().startswith("alaska_")) {
        alaska::println("inlining calls to ", F.getName());
        for (auto user : F.users()) {
          if (auto call = dyn_cast<CallInst>(user)) {
            if (call->getCalledFunction() != &F) continue;

            llvm::InlineFunctionInfo IFI;
            llvm::InlineFunction(*call, IFI);
          }
        }
      }
    }
    // inlineCallsTo(M.getFunction("alaska_translate"));
    // inlineCallsTo(M.getFunction("alaska_release"));
    // replace_function(M, "alaska_translate", "alaska_prefetch_translate");
    // replace_function(M, "alaska_release", "alaska_prefetch_release");
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
          // Run a normalization pass regardless of the environment configuration
          MPM.addPass(AlaskaNormalizePass());

          if (getenv("ALASKA_COMPILER_BASELINE") == NULL) {
            if (getenv("ALASKA_ONLY_PREFETCH") != NULL) {
              MPM.addPass(AlaskaTranslatePass());
              MPM.addPass(PrefetchPass());
              MPM.addPass(AlaskaLinkLibraryPass(ALASKA_INSTALL_PREFIX "/lib/alaska_prefetch.bc"));
              return true;
            }


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

#ifdef ALASKA_DUMP_TRANSLATIONS
            MPM.addPass(LockPrinterPass());
#endif
            // MPM.addPass(RedundantArgumentLockElisionPass());
            if (!alaska::bootstrapping()) MPM.addPass(LockInsertionPass());

#ifdef ALASKA_INLINE_TRANSLATION
            MPM.addPass(AlaskaLinkLibraryPass(ALASKA_INSTALL_PREFIX "/lib/alaska_translate.bc"));
#endif

            MPM.addPass(AlaskaLowerPass());

#ifdef ALASKA_INLINE_TRANSLATION
            // Force inlines of alaska runtime functions
            MPM.addPass(TranslationInlinePass());
#endif
          }


          return true;
        });
      }};
}
