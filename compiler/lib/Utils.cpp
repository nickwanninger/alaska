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


llvm::Value *alaska::insertRTCall(InsertionType type, llvm::Value *handle, llvm::Instruction *inst) {
  using namespace llvm;

  // The original basic block
  auto *headBB = inst->getParent();
  auto &F = *headBB->getParent();
  auto &M = *F.getParent();
  LLVMContext &ctx = M.getContext();
  auto ptrType = PointerType::get(ctx, 0);

  IRBuilder<> b(inst);

  // Insert a call to `alaska_get` and return the getned pointer
  if (type == InsertionType::Get) {
    auto getFunctionType = FunctionType::get(ptrType, {ptrType}, false);
    auto getFunction = M.getOrInsertFunction("alaska_get", getFunctionType).getCallee();
    return b.CreateCall(getFunctionType, getFunction, {handle});
  }

  // Insert a call to `alaska_put`, and return nothing
  if (type == InsertionType::Put) {
    auto putFunctionType = FunctionType::get(Type::getVoidTy(ctx), {ptrType}, false);
    auto putFunction = M.getOrInsertFunction("alaska_put", putFunctionType).getCallee();
    b.CreateCall(putFunctionType, putFunction, {handle});
    return nullptr;
  }

  return NULL;
}


// Insert the call to alaska_get or alaska_put, but inline the handle guard
// as a new basic block. This improves performance quite a bit :)
llvm::Value *alaska::insertGuardedRTCall(InsertionType type, llvm::Value *handle, llvm::Instruction *inst) {
  using namespace llvm;

  // The original basic block
  auto *headBB = inst->getParent();
  auto &F = *headBB->getParent();
  auto &M = *F.getParent();
  LLVMContext &ctx = M.getContext();
  auto ptrType = PointerType::get(ctx, 0);

  IRBuilder<> b(inst);
  auto sltInst = b.CreateICmpSLT(handle, ConstantPointerNull::get(ptrType));


  // a pointer to the terminator of the "Then" block
  auto getTerminator = SplitBlockAndInsertIfThen(sltInst, inst, false);
  auto termBB = dyn_cast<BranchInst>(getTerminator)->getSuccessor(0);

  // Insert a call to `alaska_get` and return the getned pointer
  if (type == InsertionType::Get) {
    auto getFunctionType = FunctionType::get(ptrType, {ptrType}, false);
    auto getFunction = M.getOrInsertFunction("alaska_guarded_get", getFunctionType).getCallee();
    b.SetInsertPoint(getTerminator);
    auto getned = b.CreateCall(getFunctionType, getFunction, {handle});
    // Create a PHI node between `handle` and `getned`
    b.SetInsertPoint(termBB->getFirstNonPHI());
    auto phiNode = b.CreatePHI(ptrType, 2);

    phiNode->addIncoming(handle, headBB);
    phiNode->addIncoming(getned, getned->getParent());

    return phiNode;
  }

  // Insert a call to `alaska_put`, and return nothing
  if (type == InsertionType::Put) {
    auto putFunctionType = FunctionType::get(Type::getVoidTy(ctx), {ptrType}, false);
    auto putFunction = M.getOrInsertFunction("alaska_guarded_put", putFunctionType).getCallee();
    b.SetInsertPoint(getTerminator);
    b.CreateCall(putFunctionType, putFunction, {handle});
    return nullptr;
  }

  return NULL;
}



static inline llvm::Value *wrapped_insert_runtime(alaska::InsertionType type, llvm::Value *handle, llvm::Instruction *inst) {
#ifdef ALASKA_INLINE_HANDLE_GUARD
  return insertGuardedRTCall(type, handle, inst);
#else
  return insertRTCall(type, handle, inst);
#endif
}


void alaska::insertConservativeTranslations(alaska::PointerFlowGraph &G) {
  // Naively insert get/put around loads and stores (the sinks in the graph provided)
  auto nodes = G.get_nodes();
  // Loop over all the nodes...
  for (auto node : nodes) {
    // only operate on sinks...
    if (node->type != alaska::Sink) continue;
    auto I = dyn_cast<Instruction>(node->value);

    // Insert the get/put.
    // We have to handle load and store seperately, as their operand ordering is different (annoyingly...)
    if (auto *load = dyn_cast<LoadInst>(I)) {
      auto ptr = load->getPointerOperand();
      auto getned = wrapped_insert_runtime(alaska::InsertionType::Get, ptr, I);
      load->setOperand(0, getned);
      wrapped_insert_runtime(alaska::InsertionType::Put, ptr, I->getNextNode());
      continue;
    }

    if (auto *store = dyn_cast<StoreInst>(I)) {
      auto ptr = store->getPointerOperand();
      auto getned = wrapped_insert_runtime(alaska::InsertionType::Get, ptr, I);
      store->setOperand(1, getned);
      wrapped_insert_runtime(alaska::InsertionType::Put, ptr, I->getNextNode());
      continue;
    }
  }
}
