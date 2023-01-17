// Alaska includes
#include <LockForest.h>
#include <Graph.h>
#include <Utils.h>
#include <LockForestTransformation.h>

// C++ includes
#include <cassert>
#include <unordered_set>

// LLVM includes
#include "llvm/IR/Operator.h"
#include "llvm/IR/InstVisitor.h"
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


#include "noelle/core/DataFlow.hpp"
#include "noelle/core/MetadataManager.hpp"

#include <optional>

class ProgressPass : public PassInfoMixin<ProgressPass> {
 public:
  const char *message;
  ProgressPass(const char *message) : message(message) {}
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
    printf("message: %s\n", message);
    return PreservedAnalyses::all();
  }
};


class AlaskaReplacementPass : public PassInfoMixin<AlaskaReplacementPass> {
 public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
    alaska::runReplacementPass(M);
    return PreservedAnalyses::none();
  }
};

class AlaskaTranslatePass : public PassInfoMixin<AlaskaTranslatePass> {
 public:
  bool hoist = false;
  AlaskaTranslatePass(bool hoist) : hoist(hoist) {}
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
		hoist = false;
    llvm::noelle::MetadataManager mdm(M);
    if (mdm.doesHaveMetadata("alaska")) return PreservedAnalyses::all();
    mdm.addMetadata("alaska", "did run");

    for (auto &F : M) {
      if (F.empty()) continue;
      auto section = F.getSection();
      // Skip functions which are annotated with `alaska_rt`
      if (section.startswith("$__ALASKA__")) {
        F.setSection("");
        continue;
      }
      if (hoist) {
        errs() << "hoisting in " << F.getName() << "\n";
        alaska::LockForestTransformation fftx(F);
        fftx.apply();
      } else {
        errs() << "not hoisting in " << F.getName() << "\n";
        alaska::PointerFlowGraph graph(F);
        alaska::insertConservativeTranslations(graph);
      }

			errs() << F << "\n";
    }

    return PreservedAnalyses::none();
  }
};

class AlaskaLinkLibrary : public PassInfoMixin<AlaskaLinkLibrary> {
 public:
  void prepareLibrary(Module &M) {
    auto linkage = GlobalValue::WeakAnyLinkage;

    for (auto &G : M.globals()) {
      if (!G.isDeclaration()) G.setLinkage(linkage);
    }

    for (auto &A : M.aliases()) {
      if (!A.isDeclaration()) A.setLinkage(linkage);
    }

    for (auto &F : M) {
      if (!F.isDeclaration()) F.setLinkage(linkage);
      F.setLinkage(linkage);
    }

    // auto lockFunc = M.getFunction("alaska_lock");
    // auto unlockFunc = M.getFunction("alaska_unlock");
    // ALASKA_SANITY(lockFunc != NULL, "unable to find alaska_lock in the library");
    // ALASKA_SANITY(unlockFunc != NULL, "unable to find alaska_unlock in the library");
    // errs() << *lockFunc << "\n";
    // errs() << *unlockFunc << "\n";
  }
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
    // link the alaska.bc file

    auto buf = llvm::MemoryBuffer::getFile("local/lib/alaska_inline_lock.bc");
    auto other = llvm::parseBitcodeFile(*buf.get(), M.getContext());

    auto other_module = std::move(other.get());
    prepareLibrary(*other_module);

    // In alaska, we only care about inlining alaska_lock and alaska_unlock.
    // here, we remove all the globals (unconditinally) and the functions which
    // are not called by alaska_{un,}lock or the functions called


    // for (auto &G : other_module->globals()) {
    //   if (auto *variable = dyn_cast<GlobalVariable>(&G)) {
    //     errs() << "global: " << *variable << "\n";
    //     // variable->setInitializer(nullptr);
    //     // variable->setLinkage(GlobalVariable::ExternalLinkage);
    //   }
    // }
    //
    // for (auto &F : *other_module) {
    //   errs() << "function: " << F.getName() << "\n";
    // }

    llvm::Linker::linkModules(M, std::move(other_module));

    return PreservedAnalyses::none();
  }
};

class AlaskaReoptimize : public PassInfoMixin<AlaskaReoptimize> {
 public:
  OptimizationLevel optLevel;
  AlaskaReoptimize(OptimizationLevel optLevel) : optLevel(optLevel) {}
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
    // Create the analysis managers.
    LoopAnalysisManager LAM;
    FunctionAnalysisManager FAM;
    CGSCCAnalysisManager CGAM;
    ModuleAnalysisManager MAM;

    // Create the new pass manager builder.
    // Take a look at the PassBuilder constructor parameters for more
    // customization, e.g. specifying a TargetMachine or various debugging
    // options.
    PassBuilder PB;

    // Register all the basic analyses with the managers.
    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

    return PB.buildPerModuleDefaultPipeline(optLevel).run(M, MAM);
  }
};




template <typename T>
auto adapt(T &&fp) {
  FunctionPassManager FPM;
  FPM.addPass(llvm::LowerInvokePass());  // Invoke sucks and we don't support it yet.
  return createModuleToFunctionPassAdaptor(std::move(FPM));
}


// Register the alaska passes with the new pass manager
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "Alaska", LLVM_VERSION_STRING, [](PassBuilder &PB) {
            PB.registerOptimizerLastEPCallback([](ModulePassManager &MPM, OptimizationLevel optLevel) {
              MPM.addPass(adapt(llvm::LowerInvokePass()));
              MPM.addPass(AlaskaReplacementPass());

              // Then Alaska specific passes
              // if (optLevel.getSpeedupLevel() == 0) {
              //   // on unoptimized builds, don't hoist
              //   MPM.addPass(AlaskaTranslatePass(false));
              // } else {
              // on optimized builds, hoist with the lock forest
              MPM.addPass(AlaskaTranslatePass(true));
              // Link the library
              MPM.addPass(AlaskaLinkLibrary());

              // attempt to inline the library stuff
              MPM.addPass(adapt(llvm::DCEPass()));
              MPM.addPass(llvm::GlobalDCEPass());
              MPM.addPass(llvm::AlwaysInlinerPass());
              // }


              // For good measures, re-optimize
              MPM.addPass(AlaskaReoptimize(optLevel));


              return true;
            });
          }};
}
