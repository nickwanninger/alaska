// Alaska includes
#include <Graph.h>
#include <Utils.h>
#include <Locks.h>

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


#include "noelle/core/DataFlow.hpp"
#include "noelle/core/MetadataManager.hpp"

#include <optional>
#include <WrappedFunctions.h>



class ProgressPass : public PassInfoMixin<ProgressPass> {
 public:
  const char *message;
  ProgressPass(const char *message) : message(message) {
  }
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
        auto l = alaska::extractLocks(F);
        if (l.size() > 0) {
          alaska::printLockDot(F, l);
        }
      }
    }
    return PreservedAnalyses::all();
  }
};


class RedundantArgumentLockElisionPass : public PassInfoMixin<RedundantArgumentLockElisionPass> {
 public:
  llvm::Value *getRootAllocation(llvm::Value *cur) {
    while (1) {
      if (auto gep = dyn_cast<GetElementPtrInst>(cur)) {
        cur = gep->getPointerOperand();
      } else {
        break;
      }
    }

    return cur;
  }

  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
    // Which arguments does each function lock internally? (Array of true and false for each index of the args)
    std::map<llvm::Function *, std::vector<bool>> locked_arguments;

    for (auto &F : M) {
      if (F.empty()) continue;
      // Which arguments do each function lock?
      auto locks = alaska::extractLocks(F);
      std::vector<bool> locked;
      locked.reserve(F.arg_size());
      for (unsigned i = 0; i < F.arg_size(); i++)
        locked.push_back(false);

      for (auto &l : locks) {
        auto lockedAllocation = getRootAllocation(l->getHandle());
        for (unsigned i = 0; i < F.arg_size(); i++) {
          auto *arg = F.getArg(i);
          if (arg == lockedAllocation) {
            locked[i] = true;
          }
        }
      }

      locked_arguments[&F] = std::move(locked);
    }

    for (auto &[func, locked] : locked_arguments) {
      alaska::println("function: ", func->getName());
      // fprintf(stderr, "%20s: ", func->getName().data());
      // alaska::print("locked args for ", func->getName(), "\t");

      alaska::print("\targs:");
      for (auto b : locked) {
        alaska::print(" ", b);
      }
      alaska::println();

      for (auto *user : func->users()) {
        if (auto *call = dyn_cast<CallInst>(user)) {
          if (call->getCalledFunction() != func) continue;

          auto *callFunction = call->getFunction();

          auto fLocks = alaska::extractLocks(*callFunction);
          alaska::print("\t use:");

          for (unsigned i = 0; i < locked.size(); i++) {
            bool alreadyLocked = false;

            auto argRoot = getRootAllocation(call->getOperand(i));

            // alaska::println(i, "lock ", *argRoot);

            for (auto &fl : fLocks) {
              auto *flRoot = getRootAllocation(fl->getHandle());
              if (flRoot == argRoot) {
                if (fl->liveInstructions.count(call) != 0) {
                  alreadyLocked = true;
                  break;
                }
              }
            }

            alaska::print(" ", alreadyLocked);
          }
          // alaska::println("\tfrom ", callFunction->getName());
          alaska::println();
          // alaska::println("   call: ", *call);
        }
      }
    }

    return PreservedAnalyses::none();
  }
};



class LockTrackerPass : public PassInfoMixin<LockTrackerPass> {
 public:
  GetElementPtrInst *CreateGEP(
      LLVMContext &Context, IRBuilder<> &B, Type *Ty, Value *BasePtr, int Idx, const char *Name) {
    Value *Indices[] = {
        ConstantInt::get(Type::getInt32Ty(Context), 0), ConstantInt::get(Type::getInt32Ty(Context), Idx)};
    Value *Val = B.CreateGEP(Ty, BasePtr, Indices, Name);

    assert(isa<GetElementPtrInst>(Val) && "Unexpected folded constant");

    return dyn_cast<GetElementPtrInst>(Val);
  }

  GetElementPtrInst *CreateGEP(
      LLVMContext &Context, IRBuilder<> &B, Type *Ty, Value *BasePtr, int Idx, int Idx2, const char *Name) {
    Value *Indices[] = {ConstantInt::get(Type::getInt32Ty(Context), 0),
        ConstantInt::get(Type::getInt32Ty(Context), Idx), ConstantInt::get(Type::getInt32Ty(Context), Idx2)};
    Value *Val = B.CreateGEP(Ty, BasePtr, Indices, Name);

    assert(isa<GetElementPtrInst>(Val) && "Unexpected folded constant");

    return dyn_cast<GetElementPtrInst>(Val);
  }


  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
    std::vector<Type *> EltTys;

    auto pointerType = llvm::PointerType::get(M.getContext(), 0);
    auto i64Ty = IntegerType::get(M.getContext(), 64);
    auto stackEntryTy = StructType::create(M.getContext(), "alaska_stackentry");
    EltTys.clear();
    EltTys.push_back(PointerType::getUnqual(stackEntryTy));  // ptr
    EltTys.push_back(i64Ty);                                 // i64
    stackEntryTy->setBody(EltTys);

    // Get the root chain if it already exists.
    auto Head = M.getGlobalVariable("alaska_lock_root_chain");
    if (Head == nullptr) {
      Head = new GlobalVariable(M, pointerType, false, GlobalValue::ExternalWeakLinkage,
          Constant::getNullValue(pointerType), "alaska_lock_root_chain", nullptr,
          llvm::GlobalValue::InitialExecTLSModel);
    }
    Head->setThreadLocal(true);
    Head->setLinkage(GlobalValue::ExternalWeakLinkage);
    Head->setInitializer(NULL);


    for (auto &F : M) {
      if (F.empty()) continue;
      auto l = alaska::extractLocks(F);
      if (l.empty()) continue;
      EltTys.clear();
      EltTys.push_back(stackEntryTy);
      // enough spots for each pointerType
      for (size_t i = 0; i < l.size(); i++) {
        EltTys.push_back(pointerType);
      }
      auto *concreteStackEntryTy = StructType::create(EltTys, ("alaska_stackentry." + F.getName()).str());

      IRBuilder<> atEntry(F.front().getFirstNonPHI());
      auto *stackEntry = atEntry.CreateAlloca(concreteStackEntryTy, nullptr, "lockStackFrame");
      atEntry.CreateStore(ConstantAggregateZero::get(concreteStackEntryTy), stackEntry, true);


      auto *EntryCountPtr = CreateGEP(M.getContext(), atEntry, stackEntryTy, stackEntry, 1, "lockStackFrame.count");
      // cur->count = N;
      atEntry.CreateStore(ConstantInt::get(i64Ty, l.size()), EntryCountPtr, true);
      // cur->prev = head;
      auto *CurrentHead = atEntry.CreateLoad(stackEntryTy->getPointerTo(), Head, "lockStackCurrentHead");
      atEntry.CreateStore(CurrentHead, stackEntry, true);
      // head = cur;
      atEntry.CreateStore(stackEntry, Head, true);

      int ind = 0;
      for (auto &lock : l) {
        char buf[512];
        snprintf(buf, 512, "lockCell.%d", ind);
        auto cell = CreateGEP(M.getContext(), atEntry, concreteStackEntryTy, stackEntry, ind + 1, buf);
        ind++;

        auto handle = lock->getHandle();

        IRBuilder<> b(lock->lock);
        b.SetInsertPoint(lock->lock);
        b.CreateStore(handle, cell, true);

        // Insert a store right after all the unlocks to say "we're done with this handle".
        for (auto unlock : lock->unlocks) {
          b.SetInsertPoint(unlock->getNextNode());
          b.CreateStore(llvm::ConstantPointerNull::get(dyn_cast<PointerType>(handle->getType())), cell, true);
        }
      }

      // For each instruction that escapes...
      EscapeEnumerator EE(F, "alaska_cleanup", /*HandleExceptions=*/true, nullptr);
      while (IRBuilder<> *AtExit = EE.Next()) {
        // Pop the entry from the shadow stack. Don't reuse CurrentHead from
        // AtEntry, since that would make the value live for the entire function.
        Value *SavedHead = AtExit->CreateLoad(stackEntryTy->getPointerTo(), stackEntry, "alaska_savedhead");
        AtExit->CreateStore(SavedHead, Head);
        // AtExit->CreateStore(CurrentHead, Head);
      }
    }

    return PreservedAnalyses::none();
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
    std::set<llvm::SelectInst *> selects;

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
    std::set<std::string> functions_to_ignore = {
        "halloc",
        "hrealloc",
        "hrealloc_trace",
        "hcalloc",
        "hfree",
        "hfree_trace",
        "alaska_lock",
        "alaska_lock_trace",
        "alaska_unlock",
        "alaska_unlock_trace",
        "alaska_classify",
        "alaska_classify_trace",
        "anchorage_manufacture_locality",
        "strstr",
        "strchr",
    };

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
      int i = -1;
      for (auto &arg : call->args()) {
        i++;
        if (!arg->getType()->isPointerTy()) continue;
        if (dyn_cast<GlobalValue>(arg)) continue;

        IRBuilder<> b(call);

        auto val = b.CreateGEP(arg->getType(), arg, {});
        auto translated = alaska::insertLockBefore(call, val);
        alaska::insertUnlockBefore(call->getNextNode(), val);
        call->setArgOperand(i, translated);
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
  AlaskaTranslatePass() {
  }
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
    llvm::noelle::MetadataManager mdm(M);
    if (mdm.doesHaveMetadata("alaska")) {
      alaska::println("Alaska has already run on this module!\n");
      return PreservedAnalyses::all();
    }

    mdm.addMetadata("alaska", "did run");

    for (auto &F : M) {
      if (F.empty()) continue;
      auto section = F.getSection();
      // Skip functions which are annotated with `alaska_rt`
      if (section.startswith("$__ALASKA__")) {
        F.setSection("");
        continue;
      }

      // alaska::println("running translate on ", F.getName());
#ifdef ALASKA_HOIST_LOCKS
      alaska::insertHoistedLocks(F);
#else
      alaska::insertConservativeLocks(F);
#endif
    }


    return PreservedAnalyses::none();
  }
};

class AlaskaLinkLibraryPass : public PassInfoMixin<AlaskaLinkLibraryPass> {
 public:
  const char *lib_path = NULL;
  GlobalValue::LinkageTypes linkage;

  AlaskaLinkLibraryPass(const char *lib_path, GlobalValue::LinkageTypes linkage = GlobalValue::WeakAnyLinkage)
      : lib_path(lib_path), linkage(linkage) {
  }
  void prepareLibrary(Module &M) {
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
    auto buf = llvm::MemoryBuffer::getFile(lib_path);
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
  AlaskaReoptimizePass(OptimizationLevel optLevel) : optLevel(optLevel) {
  }
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
              // MPM.addPass(AlaskaLinkLibraryPass(ALASKA_INSTALL_PREFIX "/lib/alaska_compat.bc"));

              if (getenv("ALASKA_COMPILER_BASELINE") == NULL) {
                MPM.addPass(ProgressPass("Normalize"));
                MPM.addPass(AlaskaNormalizePass());
                if (!alaska::bootstrapping()) {
                  // run replacement on non-bootstrapped code
                  MPM.addPass(ProgressPass("Replacement"));

                  MPM.addPass(AlaskaReplacementPass());
                }
                MPM.addPass(ProgressPass("Translate"));

                MPM.addPass(AlaskaTranslatePass());

#ifdef ALASKA_ESCAPE_PASS
                if (!alaska::bootstrapping()) {
                  MPM.addPass(AlaskaEscapePass());
                }
#endif
                // MPM.addPass(LockRemoverPass());
                // MPM.addPass(RedundantArgumentLockElisionPass());
                MPM.addPass(ProgressPass("LockTracker"));
                MPM.addPass(LockTrackerPass());

#ifdef ALASKA_DUMP_LOCKS
                MPM.addPass(LockPrinterPass());
#endif
                MPM.addPass(ProgressPass("Linking"));

                if (alaska::bootstrapping()) {
                  // Use the bootstrap bitcode if we are bootstrapping
                  MPM.addPass(AlaskaLinkLibraryPass(ALASKA_INSTALL_PREFIX "/lib/alaska_bootstrap.bc"));
                } else {
                  // Link the library otherwise (just runtime/src/lock.c)
                  MPM.addPass(AlaskaLinkLibraryPass(ALASKA_INSTALL_PREFIX "/lib/alaska_lock.bc"));
                }
              }

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
