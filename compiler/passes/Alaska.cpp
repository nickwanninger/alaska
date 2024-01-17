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
#include "llvm/Analysis/InlineAdvisor.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/Support/GraphWriter.h"
#include "llvm/Transforms/Scalar/DCE.h"
#include "llvm/Transforms/Scalar/ADCE.h"
#include "llvm/Transforms/Scalar/SCCP.h"
#include "llvm/IR/PassTimingInfo.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"
#include "llvm/Transforms/IPO/WholeProgramDevirt.h"
#include "llvm/Transforms/Utils/SCCPSolver.h"
#include "llvm/Transforms/Utils/PredicateInfo.h"

#include "llvm/Bitcode/BitcodeWriter.h"
// Noelle Includes
#include <noelle/core/DataFlow.hpp>
#include <noelle/core/CallGraph.hpp>
#include <noelle/core/MetadataManager.hpp>
#include <alaska/Linker.h>

// C++ includes
#include <cassert>
#include <set>
#include <optional>


#include <noelle/core/DGBase.hpp>



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
      if (verifyFunction(F, &errs())) {
        errs() << "Function verification failed!\n";
        errs() << F.getName() << "\n";
        auto l = alaska::extractTranslations(F);
        if (l.size() > 0) {
          alaska::printTranslationDot(F, l);
        }
        errs() << F << "\n";
        exit(EXIT_FAILURE);
      }
      if (F.getName() == "inc") alaska::println(F);
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



std::vector<llvm::Value *> getAllValues(llvm::Module &M) {
  std::vector<llvm::Value *> vals;

  for (auto &F : M) {
    // vals.push_back(&F);
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



class LTOTestPass : public llvm::PassInfoMixin<LTOTestPass> {
 public:
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &AM) {
    // First, write the module to disk as a bitcode file
    std::error_code EC;
    llvm::raw_fd_stream f("module.bc", EC);
    llvm::WriteBitcodeToFile(M, f);
    f.flush();  // Make sure to flush it.
    system("llvm-dis module.bc");


    system("alaska-transform module.bc -o module.bc");

    M.getFunctionList().clear();
    M.getGlobalList().clear();


    auto buf = llvm::MemoryBuffer::getFile("module.bc");
    auto other = llvm::parseBitcodeFile(*buf.get(), M.getContext());
    if (!other) {
      alaska::println("Error parsing module: ", other.takeError());
      exit(EXIT_FAILURE);
    }
    auto other_module = std::move(other.get());

    if (other_module->materializeMetadata()) {
      fprintf(stderr, "Could not materialize metadata\n");
      exit(EXIT_FAILURE);
    }



    unsigned ApplicableFlags = Linker::Flags::OverrideFromSrc;
    alaska::Linker L(M);
    L.linkInModule(std::move(other_module), ApplicableFlags, nullptr);

    alaska::println("================= After ================");
    alaska::println(M);

    return PreservedAnalyses::all();
  }
};


template <typename T>
auto adapt(T &&fp) {
  FunctionPassManager FPM;
  FPM.addPass(std::move(fp));
  return createModuleToFunctionPassAdaptor(std::move(FPM));
}


#define REGISTER(passName, PassType) \
  if (name == passName) {            \
    MPM.addPass(PassType());         \
    return true;                     \
  }


static bool add_pass(StringRef name, ModulePassManager &MPM) {
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
    MPM.addPass(adapt(PlaceSafepointsPass()));
    MPM.addPass(PinTrackingPass());
    return true;
  }
  return false;
}



// Register the alaska passes with the new pass manager
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "Alaska", LLVM_VERSION_STRING,  //
      [](PassBuilder &PB) {
        PB.registerOptimizerLastEPCallback([](ModulePassManager &MPM, OptimizationLevel) {
          printf("OPT Last!\n");
          // MPM.addPass(LTOTestPass());
          // add_pass("alaska-prepare", MPM);
          // add_pass("alaska-replace", MPM);
          // add_pass("alaska-translate", MPM);
        });

        PB.registerFullLinkTimeOptimizationLastEPCallback(
            [](ModulePassManager &MPM, OptimizationLevel) {
              printf("LTO Last!\n");
              MPM.addPass(LTOTestPass());
              // add_pass("alaska-escape", MPM);
              // add_pass("alaska-lower", MPM);
              // add_pass("alaska-inline", MPM);
            });


        PB.registerPipelineParsingCallback([](StringRef name, ModulePassManager &MPM,
                                               ArrayRef<llvm::PassBuilder::PipelineElement>) {
          return add_pass(name, MPM);
        });
      }};
}
