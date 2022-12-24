#include <Utils.h>


#include "llvm/IR/Operator.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"


llvm::Instruction *alaska::insertLockBefore(llvm::Instruction *inst, llvm::Value *pointer) {
  auto *headBB = inst->getParent();
  auto &F = *headBB->getParent();
  auto &M = *F.getParent();
  LLVMContext &ctx = M.getContext();
  auto ptrType = PointerType::get(ctx, 0);
  auto lockFunctionType = FunctionType::get(ptrType, {ptrType}, false);
  auto lockFunction = M.getOrInsertFunction("alaska_lock", lockFunctionType).getCallee();

  IRBuilder<> b(inst);
  auto locked = b.CreateCall(lockFunctionType, lockFunction, {pointer});
	locked->setName("locked");
	return locked;
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
      auto t = insertGuardedRTCall(alaska::InsertionType::Lock, ptr, inst, dbg);
      load->setOperand(0, t);
      insertGuardedRTCall(alaska::InsertionType::Unlock, ptr, inst->getNextNode(), dbg);
      continue;
    }

    if (auto *store = dyn_cast<StoreInst>(inst)) {
      auto ptr = store->getPointerOperand();
      auto t = insertGuardedRTCall(alaska::InsertionType::Lock, ptr, inst, dbg);
      store->setOperand(1, t);
      insertGuardedRTCall(alaska::InsertionType::Unlock, ptr, inst->getNextNode(), dbg);
      continue;
    }
  }
}
