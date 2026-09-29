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

#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/IntrinsicInst.h>
#include "llvm/BinaryFormat/Dwarf.h"


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
      if (F.empty()) continue;
      if (F.getName().starts_with("alaska_")) {
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

      if (F.getName().starts_with(".omp_outlined")) continue;
      if (F.getName().starts_with("omp_outlined")) continue;

      llvm::DominatorTree DT(F);
      llvm::PostDominatorTree PDT(F);
      llvm::LoopInfo loops(DT);

      bool hasProblematicCalls = false;

      for (auto &BB : F) {
        for (auto &I : BB) {
          if (auto *call = dyn_cast<CallInst>(&I)) {
            auto *calledFunction = call->getCalledFunction();
            // Is it an intrinsic?
            if (calledFunction->getName().starts_with("llvm.")) continue;

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
    // ot.dump();

    return PreservedAnalyses::all();
  }
};

class AlaskaTypeEmissionPass : public llvm::PassInfoMixin<AlaskaTypeEmissionPass> {
  struct StructTypes {
    llvm::StructType *TypeInfo;
    llvm::StructType *TypeMember;
  };

  StructTypes getOrCreateStructTypes(llvm::Module &M) {
    llvm::LLVMContext &C = M.getContext();
    llvm::StructType *TypeInfo = llvm::StructType::getTypeByName(C, "struct.alaska_typeinfo");
    llvm::StructType *TypeMember = llvm::StructType::getTypeByName(C, "struct.alaska_typemember");

    if (!TypeInfo) {
      TypeInfo = llvm::StructType::create(C, "struct.alaska_typeinfo");
    }
    if (!TypeMember) {
      TypeMember = llvm::StructType::create(C, "struct.alaska_typemember");
    }

    // struct alaska_typemember { const char *name; size_t offset; alaska_typeinfo_t *type; };
    if (TypeMember->isOpaque()) {
      TypeMember->setBody({
          llvm::PointerType::getUnqual(C),        // name
          llvm::Type::getInt64Ty(C),              // offset
          llvm::PointerType::getUnqual(TypeInfo)  // type
      });
    }

    // struct alaska_typeinfo { char* name; size_t size; long flags; size_t count; member* members;
    // }
    if (TypeInfo->isOpaque()) {
      TypeInfo->setBody({
          llvm::PointerType::getUnqual(C),          // name
          llvm::Type::getInt64Ty(C),                // byte_size
          llvm::Type::getInt64Ty(C),                // flags
          llvm::Type::getInt64Ty(C),                // member_count
          llvm::PointerType::getUnqual(TypeMember)  // members
      });
    }

    return {TypeInfo, TypeMember};
  }

  llvm::DenseMap<llvm::DIType *, llvm::GlobalVariable *> TypeCache;

  llvm::GlobalVariable *getOrCreateTypeInfo(llvm::DIType *T, llvm::Module &M, StructTypes &ST) {
    if (!T) return nullptr;
    if (TypeCache.count(T)) return TypeCache[T];

    llvm::LLVMContext &C = M.getContext();

    // Create the global variable first to handle recursion (cycles)
    auto *GV = new llvm::GlobalVariable(M, ST.TypeInfo, true, llvm::GlobalValue::InternalLinkage,
                                        nullptr, "alaska_type_" + T->getName());
    TypeCache[T] = GV;

    // Build the initializer
    llvm::Constant *Name = getConstantString(M, T->getName());
    uint64_t Size = T->getSizeInBits() / 8;  // bits to bytes
    uint64_t Flags = 0;

    std::vector<llvm::Constant *> Members;

    if (auto *Composite = llvm::dyn_cast<llvm::DICompositeType>(T)) {
      for (llvm::DINode *El : Composite->getElements()) {
        if (auto *Member = llvm::dyn_cast<llvm::DIDerivedType>(El)) {
          if (Member->getTag() == llvm::dwarf::DW_TAG_member) {
            llvm::DIType *MemberType = Member->getBaseType();
            llvm::GlobalVariable *MemberTypeGV = getOrCreateTypeInfo(MemberType, M, ST);

            // If member type is null (e.g. void* or unknown), simplify handling or skip?
            // For now, if null, we might put a null pointer or similar.
            // alaska_typemember { name, offset, type* }

            llvm::Constant *MemName = getConstantString(M, Member->getName());
            llvm::Constant *MemOffset =
                llvm::ConstantInt::get(llvm::Type::getInt64Ty(C), Member->getOffsetInBits() / 8);
            llvm::Constant *MemTypePtr =
                MemberTypeGV
                    ? llvm::ConstantExpr::getPointerCast(MemberTypeGV,
                                                         llvm::PointerType::getUnqual(ST.TypeInfo))
                    : llvm::Constant::getNullValue(llvm::PointerType::getUnqual(ST.TypeInfo));

            Members.push_back(
                llvm::ConstantStruct::get(ST.TypeMember, {MemName, MemOffset, MemTypePtr}));
          }
        }
      }
    }

    llvm::Constant *MemberArray = nullptr;
    if (!Members.empty()) {
      auto *ArrayTy = llvm::ArrayType::get(ST.TypeMember, Members.size());
      auto *ArrayInit = llvm::ConstantArray::get(ArrayTy, Members);
      auto *ArrayGV = new llvm::GlobalVariable(M, ArrayTy, true, llvm::GlobalValue::InternalLinkage,
                                               ArrayInit, "alaska_members_" + T->getName());
      MemberArray =
          llvm::ConstantExpr::getPointerCast(ArrayGV, llvm::PointerType::getUnqual(ST.TypeMember));
    } else {
      MemberArray = llvm::Constant::getNullValue(llvm::PointerType::getUnqual(ST.TypeMember));
    }

    llvm::Constant *SizeConst = llvm::ConstantInt::get(llvm::Type::getInt64Ty(C), Size);
    llvm::Constant *FlagsConst = llvm::ConstantInt::get(llvm::Type::getInt64Ty(C), Flags);
    llvm::Constant *CountConst = llvm::ConstantInt::get(llvm::Type::getInt64Ty(C), Members.size());

    GV->setInitializer(llvm::ConstantStruct::get(
        ST.TypeInfo, {Name, SizeConst, FlagsConst, CountConst, MemberArray}));

    llvm::errs() << "Created Type Info: " << T->getName() << " (Size: " << Size << ") -> @"
                 << GV->getName() << "\n";

    return GV;
  }

  llvm::Constant *getConstantString(llvm::Module &M, llvm::StringRef Str) {
    if (Str.empty())
      return llvm::Constant::getNullValue(llvm::PointerType::getUnqual(M.getContext()));

    // Helper to get or create a global string constant
    // We can use IRBuilder or just simple GlobalVariable creation
    auto *CAG = llvm::ConstantDataArray::getString(M.getContext(), Str, true);
    auto *GV = new llvm::GlobalVariable(M, CAG->getType(), true, llvm::GlobalValue::InternalLinkage,
                                        CAG, ".str");
    return llvm::ConstantExpr::getPointerCast(GV, llvm::PointerType::getUnqual(M.getContext()));
  }

 public:
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &AM) {
    TypeCache.clear();
    StructTypes ST = getOrCreateStructTypes(M);

    // Seed the worklist with all types found in the module.
    // A robust way is to walk all relevant debug info.
    llvm::SmallVector<llvm::DIType *, 32> Worklist;
    llvm::SmallPtrSet<llvm::DIType *, 32> Seen;

    auto addToWorklist = [&](llvm::DIType *T) {
      if (T && Seen.insert(T).second) Worklist.push_back(T);
    };

    // 1. Globals
    for (auto &G : M.globals()) {
      llvm::SmallVector<llvm::DIGlobalVariableExpression *, 1> GVs;
      G.getDebugInfo(GVs);
      for (auto *GVE : GVs) {
        if (auto *Var = GVE->getVariable()) addToWorklist(Var->getType());
      }
    }

    // 2. Instructions
    for (auto &F : M) {
      for (auto &BB : F) {
        for (auto &I : BB) {
          // Debug Records
          for (llvm::DbgRecord &DR : llvm::filterDbgVars(I.getDbgRecordRange())) {
            if (auto *DbgVar = llvm::dyn_cast<llvm::DbgVariableRecord>(&DR)) {
              if (auto *Var = DbgVar->getVariable()) addToWorklist(Var->getType());
            }
          }
          // Intrinsics
          if (auto *Dbg = llvm::dyn_cast<llvm::DbgVariableIntrinsic>(&I)) {
            if (auto *Var = Dbg->getVariable()) addToWorklist(Var->getType());
          }
        }
      }
    }

    // Process worklist (and recurse implicitly via getOrCreateTypeInfo, but we want to ensure roots
    // are hit) Actually getOrCreateTypeInfo recurses, so we just need to call it on roots.
    for (auto *T : Worklist) {
      getOrCreateTypeInfo(T, M, ST);
    }

    // llvm::NamedMDNode *NMD = M.getOrInsertNamedMetadata("alaska.types");
    // for (auto const &Pair : TypeCache) {
    //   llvm::DIType *Type = Pair.first;
    //   llvm::GlobalVariable *GV = Pair.second;

    //   std::vector<llvm::Metadata *> Ops;
    //   Ops.push_back(Type);
    //   Ops.push_back(llvm::ValueAsMetadata::get(GV));
    //   NMD->addOperand(llvm::MDNode::get(M.getContext(), Ops));
    // }

    return llvm::PreservedAnalyses::all();
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
                // MPM.addPass(AlaskaTypeEmissionPass());
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
                MPM.addPass(AlaskaIntentPass());
                return true;
              }

              if (name == "alaska-translate-nohoist") {
                MPM.addPass(AlaskaTranslatePass(false));
                return true;
              }

              REGISTER("alaska-escape", AlaskaEscapePass);
              if (name == "alaska-lower") {
                MPM.addPass(AlaskaLowerPass());
                return true;
              }
              REGISTER("alaska-inline", TranslationInlinePass);

              if (name == "alaska-tracking") {
#ifdef ALASKA_DUMP_TRANSLATIONS
                MPM.addPass(TranslationPrinterPass());
#endif
                MPM.addPass(llvm::PlaceSafepointsPass());
                MPM.addPass(HandleFaultPass());
                MPM.addPass(PinTrackingPass());
                return true;
              }

              return false;
            });
          }};
}
