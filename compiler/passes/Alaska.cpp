// Alaska includes
#include <Graph.h>
#include <Utils.h>
#include <Locks.h>

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
#include <WrappedFunctions.h>


class ProgressPass : public PassInfoMixin<ProgressPass> {
 public:
  const char *message;
  ProgressPass(const char *message) : message(message) {}
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
    printf("message: %s\n", message);
    return PreservedAnalyses::all();
  }
};

class LockPrinterPass : public PassInfoMixin<LockPrinterPass> {
 public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
    std::set<std::string> focus_on;
    std::string focus;

#ifdef ALASKA_DUMP_LOCKS_FOCUS
		focus = ALASKA_DUMP_LOCKS_FOCUS;
#endif

    size_t pos = 0, found;
    while ((found = focus.find(",", pos)) != std::string::npos) {
      focus_on.insert(focus.substr(pos, found - pos));
      pos = found + 1;
    }
    focus_on.insert(focus.substr(pos));

    for (auto &F : M) {
      if (focus.size() == 0 || (focus_on.find(std::string(F.getName())) != focus_on.end())) {
        errs() << F << "\n";
        auto l = alaska::extractLocks(F);
        if (l.size() > 0) {
          alaska::printLockDot(F, l);

          // errs() << F << "\n";
          // for (auto &lk : l) {
          // 	lk->remove();
          // }
          // errs() << F << "\n";
        }
      }
    }
    return PreservedAnalyses::all();
  }
};




class AlaskaNormalizePass : public PassInfoMixin<AlaskaNormalizePass> {
 public:
  // Compute parent and type for instructions before inserting them into the trace.
  class NormalizeVisitor : public llvm::InstVisitor<NormalizeVisitor> {
   public:
    llvm::Value *handleOperator(llvm::Operator *op, llvm::Instruction *inst) {
      IRBuilder<> b(inst);

      if (auto bitCastOp = dyn_cast<BitCastOperator>(op)) {
        return new llvm::BitCastInst(bitCastOp->getOperand(0), bitCastOp->getDestTy(), "", inst);
      }

      if (auto gep = dyn_cast<GEPOperator>(op)) {
        // errs() << "found gep operator\n";
        std::vector<llvm::Value *> inds(gep->idx_begin(), gep->idx_end());
        if (gep->isInBounds()) {
          gep->hasIndices();
          return llvm::GetElementPtrInst::CreateInBounds(
              gep->getSourceElementType(), gep->getPointerOperand(), inds, "", inst);
        } else {
          return llvm::GetElementPtrInst::Create(gep->getSourceElementType(), gep->getPointerOperand(), inds, "", inst);
        }
      }

      return nullptr;
    }

    void visitStoreInst(llvm::StoreInst &I) {
      if (auto opInst = dyn_cast<llvm::Instruction>(I.getPointerOperand())) {
        return;
      }
      if (auto op = dyn_cast<llvm::Operator>(I.getPointerOperand())) {
        auto n = handleOperator(op, &I);
        if (n) I.setOperand(1, n);
      }
    }

    void visitLoadInst(llvm::LoadInst &I) {
      if (auto opInst = dyn_cast<llvm::Instruction>(I.getPointerOperand())) {
        return;
      }
      if (auto op = dyn_cast<llvm::Operator>(I.getPointerOperand())) {
        auto n = handleOperator(op, &I);
        if (n) I.setOperand(0, n);
      }
    }
  };


  // Normalize select statements that have a pointer type
  void normalizeSelects(Function &F) {
    std::unordered_set<llvm::SelectInst *> selects;

    for (auto &BB : F) {
      for (auto &I : BB) {
        if (auto sinst = dyn_cast<SelectInst>(&I)) {
          if (sinst->getType()->isPointerTy()) {
            selects.insert(sinst);
          }
        }
      }
    }
    if (selects.size() == 0) return;

    for (auto *sel : selects) {
      Instruction *thenTerm, *elseTerm;
      SplitBlockAndInsertIfThenElse(sel->getCondition(), sel->getNextNode(), &thenTerm, &elseTerm);

      auto termBB = dyn_cast<BranchInst>(thenTerm)->getSuccessor(0);

      IRBuilder<> b(termBB->getFirstNonPHI());
      auto phi = b.CreatePHI(sel->getType(), 2);
      phi->addIncoming(sel->getTrueValue(), thenTerm->getParent());
      phi->addIncoming(sel->getFalseValue(), elseTerm->getParent());
      sel->replaceAllUsesWith(phi);
      sel->eraseFromParent();
    }
  }

  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
    NormalizeVisitor v;
    std::vector<llvm::Instruction *> insts;

    for (auto &F : M) {
      if (F.empty()) continue;
      for (auto &BB : F) {
        for (auto &I : BB) {
          insts.push_back(&I);
        }
      }
    }

    for (auto *I : insts)
      v.visit(I);


    for (auto &F : M)
      normalizeSelects(F);
    return PreservedAnalyses::none();
  }
};




class AlaskaEscapePass : public PassInfoMixin<AlaskaEscapePass> {
 public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
#ifndef ALASKA_ESCAPE_PASS
    return false;
#endif
    std::set<std::string> functions_to_ignore = {"halloc", "hrealloc", "hrealloc_trace", "hcalloc", "hfree",
        "hfree_trace", "alaska_lock", "alaska_lock_trace", "alaska_unlock", "alaska_unlock_trace", "alaska_classify",
        "alaska_classify_trace"};

    for (auto r : alaska::wrapped_functions) {
      functions_to_ignore.insert(r);
    }

    std::vector<llvm::CallInst *> escapes;
    for (auto &F : M) {
      // if the function is defined (has a body) skip it
      if (!F.empty()) continue;
      if (functions_to_ignore.find(std::string(F.getName())) != functions_to_ignore.end()) continue;
      if (F.getName().startswith("llvm.lifetime")) continue;
      if (F.getName().startswith("llvm.dbg")) continue;

      for (auto user : F.users()) {
        if (auto call = dyn_cast<CallInst>(user)) {
          if (call->getCalledFunction() == &F) {
            escapes.push_back(call);
          }
        }
      }
    }

    for (auto *call : escapes) {
      int i = 0;
      for (auto &arg : call->args()) {
        if (arg->getType()->isPointerTy()) {
          auto translated = alaska::insertLockBefore(call, arg);
          alaska::insertUnlockBefore(call->getNextNode(), arg);
          call->setArgOperand(i, translated);
        }
        i++;
      }
    }

    return PreservedAnalyses::none();
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
    hoist = true;
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

      alaska::println("running translate on ", F.getName());
      if (hoist) {
        alaska::insertHoistedLocks(F);
      } else {
        alaska::insertConservativeLocks(F);
      }
    }


    return PreservedAnalyses::none();
  }
};

class AlaskaLinkLibraryPass : public PassInfoMixin<AlaskaLinkLibraryPass> {
 public:
  void prepareLibrary(Module &M) {
    auto linkage = GlobalValue::WeakAnyLinkage;

    for (auto &G : M.globals())
      if (!G.isDeclaration()) G.setLinkage(linkage);

    for (auto &A : M.aliases())
      if (!A.isDeclaration()) A.setLinkage(linkage);

    for (auto &F : M) {
      if (!F.isDeclaration()) F.setLinkage(linkage);
      F.setLinkage(linkage);
    }
  }
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
    // link the alaska.bc file
    auto buf = llvm::MemoryBuffer::getFile(ALASKA_INSTALL_PREFIX "/lib/alaska_inline_lock.bc");
    auto other = llvm::parseBitcodeFile(*buf.get(), M.getContext());
    auto other_module = std::move(other.get());

    prepareLibrary(*other_module);
    llvm::Linker::linkModules(M, std::move(other_module));
    return PreservedAnalyses::none();
  }
};



class AlaskaReoptimizePass : public PassInfoMixin<AlaskaReoptimizePass> {
 public:
  OptimizationLevel optLevel;
  AlaskaReoptimizePass(OptimizationLevel optLevel) : optLevel(optLevel) {}
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
    // Create the analysis managers.
    LoopAnalysisManager LAM;
    FunctionAnalysisManager FAM;
    CGSCCAnalysisManager CGAM;
    ModuleAnalysisManager MAM;

    // Create the new pass manager builder.
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
  FPM.addPass(std::move(fp));
  return createModuleToFunctionPassAdaptor(std::move(FPM));
}


// Register the alaska passes with the new pass manager
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "Alaska", LLVM_VERSION_STRING, [](PassBuilder &PB) {
            PB.registerOptimizerLastEPCallback([](ModulePassManager &MPM, OptimizationLevel optLevel) {
              MPM.addPass(adapt(llvm::LowerInvokePass()));
              MPM.addPass(AlaskaNormalizePass());
              MPM.addPass(AlaskaReplacementPass());
              MPM.addPass(AlaskaEscapePass());

              // Insert lock calls
              MPM.addPass(AlaskaTranslatePass(optLevel.getSpeedupLevel() > 0));

      // TODO: perform the

#ifdef ALASKA_DUMP_LOCKS
              MPM.addPass(LockPrinterPass());
#endif


              // Link the library (just runtime/src/lock.c)
              MPM.addPass(AlaskaLinkLibraryPass());

              // attempt to inline the library stuff
              MPM.addPass(adapt(llvm::DCEPass()));
              MPM.addPass(llvm::GlobalDCEPass());
              MPM.addPass(llvm::AlwaysInlinerPass());

              // For good measures, re-optimize
              MPM.addPass(AlaskaReoptimizePass(optLevel));

              return true;
            });
          }};
}
