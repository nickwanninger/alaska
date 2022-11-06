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


  // Insert a call to `alaska_pin` and return the pinned pointer
  if (type == InsertionType::Pin) {
    auto pinFunctionType = FunctionType::get(ptrType, {ptrType}, false);
    auto pinFunction = M.getOrInsertFunction("alaska_pin", pinFunctionType).getCallee();
    return b.CreateCall(pinFunctionType, pinFunction, {handle});
  }

  // Insert a call to `alaska_unpin`, and return nothing
  if (type == InsertionType::UnPin) {
    auto unpinFunctionType = FunctionType::get(Type::getVoidTy(ctx), {ptrType}, false);
    auto unpinFunction = M.getOrInsertFunction("alaska_unpin", unpinFunctionType).getCallee();
    b.CreateCall(unpinFunctionType, unpinFunction, {handle});
    return nullptr;
  }

  return NULL;
}


// Insert the call to alaska_pin or alaska_unpin, but inline the handle guard
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
  auto pinTerminator = SplitBlockAndInsertIfThen(sltInst, inst, false);
  auto termBB = dyn_cast<BranchInst>(pinTerminator)->getSuccessor(0);

  // Insert a call to `alaska_pin` and return the pinned pointer
  if (type == InsertionType::Pin) {
    auto pinFunctionType = FunctionType::get(ptrType, {ptrType}, false);
    auto pinFunction = M.getOrInsertFunction("alaska_guarded_pin", pinFunctionType).getCallee();
    b.SetInsertPoint(pinTerminator);
    auto pinned = b.CreateCall(pinFunctionType, pinFunction, {handle});
    // Create a PHI node between `handle` and `pinned`
    b.SetInsertPoint(termBB->getFirstNonPHI());
    auto phiNode = b.CreatePHI(ptrType, 2);

    phiNode->addIncoming(handle, headBB);
    phiNode->addIncoming(pinned, pinned->getParent());

    return phiNode;
  }

  // Insert a call to `alaska_unpin`, and return nothing
  if (type == InsertionType::UnPin) {
    auto unpinFunctionType = FunctionType::get(Type::getVoidTy(ctx), {ptrType}, false);
    auto unpinFunction = M.getOrInsertFunction("alaska_guarded_unpin", unpinFunctionType).getCallee();
    b.SetInsertPoint(pinTerminator);
    b.CreateCall(unpinFunctionType, unpinFunction, {handle});
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


void alaska::insertConservativePins(alaska::PinGraph &G) {
  // Naively insert pin/unpin around loads and stores (the sinks in the graph provided)
  auto nodes = G.get_nodes();
  // Loop over all the nodes...
  for (auto node : nodes) {
    // only operate on sinks...
    if (node->type != alaska::Sink) continue;
    auto I = dyn_cast<Instruction>(node->value);

    // Insert the pin/unpin.
    // We have to handle load and store seperately, as their operand ordering is different (annoyingly...)
    if (auto *load = dyn_cast<LoadInst>(I)) {
      auto ptr = load->getPointerOperand();
      auto pinned = alaska::insertGuardedRTCall(alaska::InsertionType::Pin, ptr, I);
      load->setOperand(0, pinned);
      wrapped_insert_runtime(alaska::InsertionType::UnPin, ptr, I->getNextNode());
      continue;
    }

    if (auto *store = dyn_cast<StoreInst>(I)) {
      auto ptr = store->getPointerOperand();
      auto pinned = alaska::insertGuardedRTCall(alaska::InsertionType::Pin, ptr, I);
      store->setOperand(1, pinned);
      wrapped_insert_runtime(alaska::InsertionType::UnPin, ptr, I->getNextNode());
      continue;
    }
  }
}
