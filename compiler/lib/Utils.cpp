#include <Utils.h>
#include <WrappedFunctions.h>

#include <execinfo.h>

#include "llvm/IR/Operator.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"


#define BT_BUF_SIZE 100
void alaska::dumpBacktrace(void) {
  int nptrs;

  void *buffer[BT_BUF_SIZE];
  char **strings;

  nptrs = backtrace(buffer, BT_BUF_SIZE);
  fprintf(stderr, "Backtrace:\n");

  strings = backtrace_symbols(buffer, nptrs);
  if (strings == NULL) {
    perror("backtrace_symbols");
    exit(EXIT_FAILURE);
  }

  for (int j = 0; j < nptrs; j++)
    fprintf(stderr, "\x1b[92m%d\x1b[0m: %s\n", j, strings[j]);

  free(strings);
}


llvm::DILocation *getFirstDILocationInFunctionKillMe(llvm::Function *F) {
  for (auto &BB : *F) {
    for (auto &I : BB) {
      if (I.getDebugLoc()) return I.getDebugLoc();
    }
  }
  return NULL;
}

llvm::Instruction *alaska::insertLockBefore(llvm::Instruction *inst, llvm::Value *pointer) {
  auto *headBB = inst->getParent();
  auto &F = *headBB->getParent();
  auto &M = *F.getParent();
  LLVMContext &ctx = M.getContext();
  auto ptrType = PointerType::get(ctx, 0);
  auto lockFunctionType = FunctionType::get(ptrType, {ptrType}, false);
  auto lockFunction = M.getOrInsertFunction("alaska_lock", lockFunctionType).getCallee();

  IRBuilder<> b(inst);
  b.SetCurrentDebugLocation(inst->getDebugLoc());
  auto locked = b.CreateCall(lockFunctionType, lockFunction, {pointer});
  // locked->setDebugLoc(DILocation::get(ctx, 0, 0, inst->getFunction()->getSubprogram()->getScope()));
  locked->setDebugLoc(getFirstDILocationInFunctionKillMe(inst->getFunction()));
  if (!locked->getDebugLoc()) {
    // errs() << "NO DEBUG INFO in " << inst->getFunction()->getName() << "\n";
  }
  locked->setName("locked");
  return locked;
}

void alaska::insertUnlockBefore(llvm::Instruction *inst, llvm::Value *pointer) {
  auto *headBB = inst->getParent();
  auto &F = *headBB->getParent();
  auto &M = *F.getParent();
  LLVMContext &ctx = M.getContext();
  auto ptrType = PointerType::get(ctx, 0);
  auto ftype = FunctionType::get(Type::getVoidTy(ctx), {ptrType}, false);
  auto func = M.getOrInsertFunction("alaska_unlock", ftype).getCallee();

  IRBuilder<> b(inst);
  b.SetCurrentDebugLocation(inst->getDebugLoc());
  auto unlock = b.CreateCall(ftype, func, {pointer});
  unlock->setDebugLoc(getFirstDILocationInFunctionKillMe(inst->getFunction()));
}



// Insert the call to alaska_get or alaska_unlock, but inline the handle guard
// as a new basic block. This improves performance quite a bit :)
llvm::Instruction *alaska::insertGuardedRTCall(
    InsertionType type, llvm::Value *handle, llvm::Instruction *inst, llvm::DebugLoc loc) {
  using namespace llvm;

  // The original basic block
  auto *headBB = inst->getParent();
  auto &F = *headBB->getParent();
  auto &M = *F.getParent();
  LLVMContext &ctx = M.getContext();
  auto ptrType = PointerType::get(ctx, 0);

  IRBuilder<> b(inst);
  b.SetCurrentDebugLocation(loc);
  auto sltInst = b.CreateICmpSLT(handle, ConstantPointerNull::get(ptrType));


  // a pointer to the terminator of the "Then" block
  auto getTerminator = SplitBlockAndInsertIfThen(sltInst, inst, false);
  auto termBB = dyn_cast<BranchInst>(getTerminator)->getSuccessor(0);

  // Insert a call to `alaska_get` and return the getned pointer
  if (type == InsertionType::Lock) {
    auto lockFunctionType = FunctionType::get(ptrType, {ptrType}, false);
    auto lockFunction = M.getOrInsertFunction("alaska_guarded_lock", lockFunctionType).getCallee();
    b.SetInsertPoint(getTerminator);
    auto getned = b.CreateCall(lockFunctionType, lockFunction, {handle});
    getned->setDebugLoc(loc);
    // Create a PHI node between `handle` and `getned`
    b.SetInsertPoint(termBB->getFirstNonPHI());
    auto phiNode = b.CreatePHI(ptrType, 2);

    phiNode->addIncoming(handle, headBB);
    phiNode->addIncoming(getned, getned->getParent());

    return phiNode;
  }

  // Insert a call to `alaska_unlock`, and return nothing
  if (type == InsertionType::Unlock) {
    auto unlockFunctionType = FunctionType::get(Type::getVoidTy(ctx), {ptrType}, false);
    auto unlockFunction = M.getOrInsertFunction("alaska_guarded_unlock", unlockFunctionType).getCallee();
    b.SetInsertPoint(getTerminator);
    auto c = b.CreateCall(unlockFunctionType, unlockFunction, {handle});
    c->setDebugLoc(loc);
    return nullptr;
  }

  return NULL;
}




void alaska::insertConservativeTranslations(alaska::PointerFlowGraph &G) {
  // Naively insert get/unlock around loads and stores (the sinks in the graph provided)
  auto nodes = G.get_nodes();
  // Loop over all the nodes...
  for (auto node : nodes) {
    // only operate on sinks...
    if (node->type != alaska::Sink) continue;
    auto inst = dyn_cast<Instruction>(node->value);

    auto dbg = inst->getDebugLoc();
    // Insert the get/unlock.
    // We have to handle load and store seperately, as their operand ordering is different (annoyingly...)
    if (auto *load = dyn_cast<LoadInst>(inst)) {
      auto ptr = load->getPointerOperand();
      auto t = insertLockBefore(inst, ptr);
      load->setOperand(0, t);
      // TODO: unlock
      continue;
    }

    if (auto *store = dyn_cast<StoreInst>(inst)) {
      auto ptr = store->getPointerOperand();
      auto t = insertLockBefore(inst, ptr);
      store->setOperand(1, t);
      // TODO: unlock
      continue;
    }
  }
}


// Take any calls to a function called `original_name` and
// replace them with a call to `new_name` instead.
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
  // delete oldFunction;
}
void alaska::runReplacementPass(llvm::Module &M) {
  // replace
  replace_function(M, "malloc", "halloc");
  replace_function(M, "calloc", "hcalloc");
  replace_function(M, "realloc", "hrealloc");
  replace_function(M, "free", "hfree");
  replace_function(M, "malloc_usable_size", "alaska_usable_size");

  // replace_function(M, "_Znwm", "halloc");
  // replace_function(M, "_Znwj", "halloc");
  // replace_function(M, "_Znaj", "halloc");
  //
  // replace_function(M, "_ZdlPv", "hfree");
  // replace_function(M, "_ZdaPv", "hfree");
  // replace_function(M, "_ZdaPv", "hfree");

  for (auto *name : alaska::wrapped_functions) {
    replace_function(M, name);
  }
}
