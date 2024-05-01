// Alaska includes
#include <alaska/AccessAutomata.h>
#include <alaska/PointerFlowGraph.h>
#include <alaska/Utils.h>
#include <alaska/Translations.h>
#include <alaska/Passes.h>
#include <alaska/PlaceSafepoints.h>  // Stolen from LLVM
#include <alaska/OptimisticTypes.h>

// LLVM includes
#include <llvm/Pass.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>
#include <llvm/Transforms/Utils/Mem2Reg.h>
#include <llvm/Transforms/Utils/LowerInvoke.h>
#include <llvm/Transforms/Utils/LowerSwitch.h>
#include "llvm/Analysis/InlineAdvisor.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/Transforms/Scalar/DCE.h"
#include "llvm/Transforms/Scalar/ADCE.h"
#include "llvm/Transforms/Scalar/SCCP.h"
#include "llvm/Transforms/IPO/WholeProgramDevirt.h"
#include "llvm/Transforms/Utils/SCCPSolver.h"
#include "llvm/Transforms/Utils/PredicateInfo.h"

// Noelle Includes
#include <noelle/core/DataFlow.hpp>
#include <noelle/core/CallGraph.hpp>
#include <noelle/core/MetadataManager.hpp>


#include <noelle/core/DGBase.hpp>

static bool print_progress = true;
class ProgressPass : public llvm::PassInfoMixin<ProgressPass> {
 public:
  static double progress_start;

  const char *message = NULL;
  ProgressPass(const char *message)
      : message(message) {}
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


class TranslationInlinePass : public llvm::PassInfoMixin<TranslationInlinePass> {
 public:
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &AM) {
    std::vector<llvm::CallInst *> toInline;
    for (auto &F : M) {
      INTENTIIONALLY BROKEN
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



std::vector<llvm::Value *> getAllValues(llvm::Module &M) {
  std::vector<llvm::Value *> vals;

  for (auto &F : M) {
    if (F.empty()) continue;
    for (auto &arg : F.args())
      vals.push_back(&arg);

    for (auto &BB : F) {
      for (auto &I : BB) {
        vals.push_back(&I);
      }
    }
  }

  return vals;
}



class SimpleFunctionPass : public llvm::PassInfoMixin<SimpleFunctionPass> {
 public:
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &AM) {
    for (auto &F : M) {
      if (F.empty()) continue;

      if (F.getName().startswith(".omp_outlined")) continue;
      if (F.getName().startswith("omp_outlined")) continue;

      llvm::DominatorTree DT(F);
      llvm::PostDominatorTree PDT(F);
      llvm::LoopInfo loops(DT);

      bool hasProblematicCalls = false;

      for (auto &BB : F) {
        for (auto &I : BB) {
          if (auto *call = dyn_cast<CallInst>(&I)) {
            auto *calledFunction = call->getCalledFunction();
            // Is it an intrinsic?
            if (calledFunction->getName().startswith("llvm.")) continue;

            hasProblematicCalls = true;
          }
        }
      }

      if (loops.getLoopsInPreorder().size() == 0 && hasProblematicCalls == false) {
        F.addFnAttr("alaska_is_simple");
      }
    }
    return PreservedAnalyses::all();
  }
};



class OptimisticTypesPass : public llvm::PassInfoMixin<OptimisticTypesPass> {
 public:
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &AM) {
    // Simply analyze the types, then embed them.
    alaska::OptimisticTypes ot;
    ot.analyze(M);
    ot.embed();
    ot.dump();

    return PreservedAnalyses::all();
  }
};



template <typename T>
static auto adapt(T &&fp) {
  return llvm::createModuleToFunctionPassAdaptor(std::move(fp));
}


#define REGISTER(passName, PassType) \
  if (name == passName) {            \
    MPM.addPass(PassType());         \
    return true;                     \
  }

// Register the alaska passes with the new pass manager
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "Alaska", LLVM_VERSION_STRING,  //
      [](PassBuilder &PB) {
        PB.registerPipelineParsingCallback([](StringRef name, ModulePassManager &MPM,
                                               ArrayRef<llvm::PassBuilder::PipelineElement>) {
          if (name == "alaska-type-infer") {
            MPM.addPass(OptimisticTypesPass());
            return true;
          }

          if (name == "alaska-prepare") {
            MPM.addPass(adapt(DCEPass()));
            MPM.addPass(adapt(DCEPass()));
            MPM.addPass(adapt(ADCEPass()));
            MPM.addPass(WholeProgramDevirtPass());
            MPM.addPass(SimpleFunctionPass());
            MPM.addPass(AlaskaNormalizePass());
            return true;
          }

          REGISTER("alaska-replace", AlaskaReplacementPass);
          if (name == "alaska-translate") {
            MPM.addPass(AlaskaTranslatePass(true));
            return true;
          }

          if (name == "alaska-translate-nohoist") {
            MPM.addPass(AlaskaTranslatePass(false));
            return true;
          }

          REGISTER("alaska-escape", AlaskaEscapePass);
          REGISTER("alaska-lower", AlaskaLowerPass);
          REGISTER("alaska-inline", TranslationInlinePass);

          if (name == "alaska-tracking") {
#ifdef ALASKA_DUMP_TRANSLATIONS
            MPM.addPass(TranslationPrinterPass());
#endif
            MPM.addPass(llvm::PlaceSafepointsPass());
            MPM.addPass(PinTrackingPass());
            return true;
          }

          return false;
        });
      }};
}
