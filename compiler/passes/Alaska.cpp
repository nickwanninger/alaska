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
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/Transforms/Scalar/DCE.h"
#include "llvm/Transforms/Scalar/ADCE.h"
#include "llvm/Transforms/Scalar/SCCP.h"
#include "llvm/IR/PassTimingInfo.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"
#include "llvm/Transforms/IPO/WholeProgramDevirt.h"
#include "llvm/Transforms/Utils/SCCPSolver.h"
#include "llvm/Transforms/Utils/PredicateInfo.h"

// Noelle Includes
#include <noelle/core/DataFlow.hpp>
#include <noelle/core/MetadataManager.hpp>

// C++ includes
#include <cassert>
#include <set>
#include <optional>

// #include "../../runtime/include/alaska/utils.h"


#include <noelle/core/DGBase.hpp>

// // SVF
// // #include "WPA/WPAPass.h"
// #include "SVF-LLVM/LLVMModule.h"
// #include "WPA/Andersen.h"
// #include "MemoryModel/PointerAnalysis.h"
// #include "MSSA/MemSSA.h"
// #include "SVF-LLVM/SVFIRBuilder.h"

// #include "Util/SVFModule.h"
// #include "Util/PTACallGraph.h"
// #include "WPA/Andersen.h"
// #include "MemoryModel/PointerAnalysis.h"
// #include "MSSA/MemSSA.h"


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

  // for (auto &G : M.globals()) {
  //   vals.push_back(&G);
  // }

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

      // alaska::println(
      //     loops.getLoopsInPreorder().size(), "\t", hasProblematicCalls, "\t", F.getName());
    }
    return PreservedAnalyses::all();
  }
};

// class SVFTestPass : public llvm::PassInfoMixin<SVFTestPass> {
//  public:
//   llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &AM) {
//     // auto &aa = AM.getResult<AAManager>(M);
//     //
//     // auto values = getAllValues(M);
//     // printf("%zd vals\n", values.size());
//     //
//     // for (auto *v1 : values) {
//     //   alaska::println(*v1);
//     //   for (auto *v2 : values) {
//     //     if (v1 == v2) continue;
//     //     auto res = aa.alias(v1, v2);
//     //     alaska::println("   ", res, *v2);
//     //   }
//     // }
//     using namespace SVF;
//     auto mod = SVF::LLVMModuleSet::buildSVFModule(M);
//     /// Build SVFIR
//     SVF::SVFIRBuilder builder(mod);
//     auto pag = builder.build();
//     // SVF::WPAPass wpa;
//
//     auto *pta = SVF::AndersenWaveDiff::createAndersenWaveDiff(pag);
//     // pta->analyze();
//     // pag->print();
//     // return PreservedAnalyses::none();
//
//     //
//     // // SVFIR *pag = pta->getPAG();
//     for (SVFIR::iterator lit = pag->begin(), elit = pag->end(); lit != elit; ++lit) {
//       PAGNode *node1 = lit->second;
//       PAGNode *node2 = node1;
//       for (SVFIR::iterator rit = lit, erit = pag->end(); rit != erit; ++rit) {
//         node2 = rit->second;
//         if (node1 == node2) continue;
//         const SVFFunction *fun1 = node1->getFunction();
//         const SVFFunction *fun2 = node2->getFunction();
//         auto result = pta->alias(node1->getId(), node2->getId());
//
//         // auto result = wpa.alias(node1->getValue(), node2->getValue());
//         if (result == SVF::AliasResult::NoAlias) continue;
//
//         const char *str = "\e[32mMayAlias\e[0m";
//         if (result == SVF::AliasResult::NoAlias) str = "NoAlias";
//         if (result == SVF::AliasResult::MustAlias) str = "\e[31mMustAlias\e[0m";
//         if (result == SVF::AliasResult::PartialAlias) str = "PartialAlias";
//         SVFUtil::outs() << str << " var" << node1->getId() << "[" << node1->getValueName() << "@"
//                         << (fun1 == nullptr ? "" : fun1->getName()) << "] --"
//                         << " var" << node2->getId() << "[" << node2->getValueName() << "@"
//                         << (fun2 == nullptr ? "" : fun2->getName()) << "]\n";
//       }
//     }
//     // errs() << M << "\n";
//
//     return PreservedAnalyses::none();
//   }
// };




// class SpecializationPass : public llvm::PassInfoMixin<SpecializationPass> {
//  public:
//   PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
//     const DataLayout &DL = M.getDataLayout();
//     auto &FAM = AM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
//     auto GetTLI = [&FAM](Function &F) -> TargetLibraryInfo & {
//       return FAM.getResult<TargetLibraryAnalysis>(F);
//     };
//     auto GetTTI = [&FAM](Function &F) -> TargetTransformInfo & {
//       return FAM.getResult<TargetIRAnalysis>(F);
//     };
//     auto GetAC = [&FAM](Function &F) -> AssumptionCache & {
//       return FAM.getResult<AssumptionAnalysis>(F);
//     };
//     auto GetAnalysis = [&FAM](Function &F) -> AnalysisResultsForFn {
//       DominatorTree &DT = FAM.getResult<DominatorTreeAnalysis>(F);
//       return {std::make_unique<PredicateInfo>(F, DT, FAM.getResult<AssumptionAnalysis>(F)), &DT,
//           FAM.getCachedResult<PostDominatorTreeAnalysis>(F)};
//     };
//
//     if (!runFunctionSpecialization(M, DL, GetTLI, GetTTI, GetAC, GetAnalysis)) {
//       printf("failed!\n");
//       return PreservedAnalyses::all();
//     }
//
//     PreservedAnalyses PA;
//     PA.preserve<DominatorTreeAnalysis>();
//     PA.preserve<PostDominatorTreeAnalysis>();
//     PA.preserve<FunctionAnalysisManagerModuleProxy>();
//     return PA;
//   }
// };




template <typename T>
auto adapt(T &&fp) {
  FunctionPassManager FPM;
  FPM.addPass(std::move(fp));
  return createModuleToFunctionPassAdaptor(std::move(FPM));
}

void populateMPM(ModulePassManager &MPM) {
  bool baseline = getenv("ALASKA_COMPILER_BASELINE") != NULL;

  if (baseline) print_progress = false;

  // MPM.addPass(SVFTestPass());
  // return;


  // Only link the stub in non-baseline
  if (!baseline) {
    MPM.addPass(AlaskaLinkLibraryPass(ALASKA_INSTALL_PREFIX "/lib/alaska_stub.bc"));
    MPM.addPass(ProgressPass("Link Stub"));
  }



  MPM.addPass(adapt(DCEPass()));
  MPM.addPass(adapt(DCEPass()));
  MPM.addPass(adapt(ADCEPass()));
  MPM.addPass(ProgressPass("DCE"));


  MPM.addPass(WholeProgramDevirtPass());

  // Label 'simple' functions.
  MPM.addPass(SimpleFunctionPass());

  // MPM.addPass(SpecializationPass());
  // MPM.addPass(ProgressPass("Specialize"));

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


    MPM.addPass(AlaskaLinkLibraryPass(ALASKA_INSTALL_PREFIX "/lib/alaska_translate.bc"));
    MPM.addPass(ProgressPass("Link runtime"));

#ifdef ALASKA_LOCK_TRACKING
    MPM.addPass(adapt(PlaceSafepointsPass()));
    MPM.addPass(ProgressPass("Safepoint Placement"));
    MPM.addPass(PinTrackingPass());
    MPM.addPass(ProgressPass("Lock Insertion"));
#endif

#ifdef ALASKA_DUMP_TRANSLATIONS
    MPM.addPass(TranslationPrinterPass());
    MPM.addPass(ProgressPass("Lock Printing"));
#endif

    // After this point, translations can no longer be parsed.
    // Any pass which needs that information must have run by now.
    MPM.addPass(AlaskaLowerPass());
    MPM.addPass(ProgressPass("Lowering"));

#ifdef ALASKA_ARGUMENT_TRACE
    MPM.addPass(AlaskaArgumentTracePass());
    MPM.addPass(ProgressPass("Argument Trace"));
#endif

    // Force inlines of alaska runtime functions
    MPM.addPass(TranslationInlinePass());
    MPM.addPass(ProgressPass("Inline runtime"));
  }


  MPM.addPass(adapt(DCEPass()));
  MPM.addPass(adapt(DCEPass()));
  MPM.addPass(adapt(ADCEPass()));
  MPM.addPass(ProgressPass("DCE"));
}


// Register the alaska passes with the new pass manager
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "Alaska", LLVM_VERSION_STRING, [](PassBuilder &PB) {
            PB.registerOptimizerLastEPCallback(
                [](ModulePassManager &MPM, OptimizationLevel optLevel) {
                  populateMPM(MPM);
                });
          }};
}
