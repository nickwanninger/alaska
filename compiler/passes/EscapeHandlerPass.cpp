#include "llvm/Pass.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/DerivedUser.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Mangler.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Operator.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Support/CommandLine.h"

#include <WrappedFunctions.h>  // functions to blacklist from escapes
#include <Utils.h>
#define ADDR_SPACE 0


using namespace llvm;
// This pass runs by default, but can be disabled with --alaska-no-replace

namespace {
  struct AlaskaPass : public ModulePass {
    static char ID;
    llvm::Type *int64Type;
    llvm::Type *voidPtrType;
    llvm::Type *voidPtrTypePinned;
    llvm::Value *translateFunction;
    AlaskaPass() : ModulePass(ID) {}

    bool doInitialization(Module &M) override { return false; }


    bool runOnModule(Module &M) override {
      std::set<std::string> functions_to_ignore = {
          "halloc", "hrealloc", "hcalloc", "hfree", "alaska_lock", "alaska_guarded_lock", "alaska_unlock", "alaska_guarded_unlock"};


      for (auto r : alaska::wrapped_functions) {
        functions_to_ignore.insert(r);
      }

      std::vector<llvm::CallInst *> escapes;
      for (auto &F : M) {
        // if the function is defined (has a body) skip it
        if (!F.empty()) continue;
        if (functions_to_ignore.find(std::string(F.getName())) != functions_to_ignore.end()) continue;
        if (F.getName().startswith("llvm.lifetime")) continue;

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
            auto *translated = alaska::insertRTCall(alaska::InsertionType::Lock, arg, call);
            call->setArgOperand(i, translated);
            alaska::insertGuardedRTCall(alaska::InsertionType::Unlock, arg, call->getNextNode());
          }
          i++;
        }
      }

      return true;
    }
  };  // namespace

  static RegisterPass<AlaskaPass> X("alaska-escape", "Handle escapes", false /* Only looks at CFG */, false /* Analysis Pass */);

  char AlaskaPass::ID = 0;
  // static RegisterPass<AlaskaPass> X("Alaska", "Handle based memory with Alaska");

  static AlaskaPass *_PassMaker = NULL;
  static RegisterStandardPasses _RegPass1(
      PassManagerBuilder::EP_OptimizerLast, [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
        if (!_PassMaker) {
          PM.add(_PassMaker = new AlaskaPass());
        }
      });  // ** for -Ox
  static RegisterStandardPasses _RegPass2(
      PassManagerBuilder::EP_EnabledOnOptLevel0, [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
        if (!_PassMaker) {
          PM.add(_PassMaker = new AlaskaPass());
        }
      });  // ** for -O0
}  // namespace
