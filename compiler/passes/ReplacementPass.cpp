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

#define ADDR_SPACE 0


using namespace llvm;
// This pass runs by default, but can be disabled with --alaska-no-replace


cl::opt<bool> no_replace("alaska-no-replace", cl::desc("Specify output filename"), cl::value_desc("filename"));

namespace {
struct AlaskaPass : public ModulePass {
  static char ID;
  llvm::Type *int64Type;
  llvm::Type *voidPtrType;
  llvm::Type *voidPtrTypePinned;
  llvm::Value *translateFunction;
  AlaskaPass() : ModulePass(ID) {}

  bool doInitialization(Module &M) override {
    return false;
  }


	void replace_function_name(Module &M, const char *original_name, const char *new_name) {
    auto oldFunction = M.getFunction(original_name);
    if (oldFunction) {
      auto newFunction = M.getOrInsertFunction(new_name, oldFunction->getType()).getCallee();
      oldFunction->replaceAllUsesWith(newFunction);
			oldFunction->eraseFromParent();
    }
		// delete oldFunction;
  }

  bool runOnModule(Module &M) override {
		if (no_replace) return false;
		replace_function_name(M, "malloc", "halloc");
		replace_function_name(M, "calloc", "hcalloc");
		replace_function_name(M, "realloc", "hrealloc");
		replace_function_name(M, "free", "hfree");
    return true;
  }
};

  static RegisterPass<AlaskaPass> X("alaska-replace", "Replace malloc/realloc/free", false /* Only looks at CFG */, false /* Analysis Pass */);

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
} // namespace
