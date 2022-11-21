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
  if (type == InsertionType::Lock) {
    auto lockFunctionType = FunctionType::get(ptrType, {ptrType}, false);
    auto lockFunction = M.getOrInsertFunction("alaska_lock", lockFunctionType).getCallee();
    return b.CreateCall(lockFunctionType, lockFunction, {handle});
  }

  // Insert a call to `alaska_unlock`, and return nothing
  if (type == InsertionType::Unlock) {
    auto unlockFunctionType = FunctionType::get(Type::getVoidTy(ctx), {ptrType}, false);
    auto unlockFunction = M.getOrInsertFunction("alaska_unlock", unlockFunctionType).getCallee();
    b.CreateCall(unlockFunctionType, unlockFunction, {handle});
    return nullptr;
  }

  return NULL;
}


// Insert the call to alaska_get or alaska_unlock, but inline the handle guard
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
  if (type == InsertionType::Lock) {
    auto lockFunctionType = FunctionType::get(ptrType, {ptrType}, false);
    auto lockFunction = M.getOrInsertFunction("alaska_guarded_lock", lockFunctionType).getCallee();
    b.SetInsertPoint(getTerminator);
    auto getned = b.CreateCall(lockFunctionType, lockFunction, {handle});
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
    b.CreateCall(unlockFunctionType, unlockFunction, {handle});
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
  // Naively insert get/unlock around loads and stores (the sinks in the graph provided)
  auto nodes = G.get_nodes();
  // Loop over all the nodes...
  for (auto node : nodes) {
    // only operate on sinks...
    if (node->type != alaska::Sink) continue;
    auto I = dyn_cast<Instruction>(node->value);

    // Insert the get/unlock.
    // We have to handle load and store seperately, as their operand ordering is different (annoyingly...)
    if (auto *load = dyn_cast<LoadInst>(I)) {
      auto ptr = load->getPointerOperand();
      auto t = wrapped_insert_runtime(alaska::InsertionType::Lock, ptr, I);
      load->setOperand(0, t);
      wrapped_insert_runtime(alaska::InsertionType::Unlock, ptr, I->getNextNode());
      continue;
    }

    if (auto *store = dyn_cast<StoreInst>(I)) {
      auto ptr = store->getPointerOperand();
      auto t = wrapped_insert_runtime(alaska::InsertionType::Lock, ptr, I);
      store->setOperand(1, t);
      wrapped_insert_runtime(alaska::InsertionType::Unlock, ptr, I->getNextNode());
      continue;
    }
  }
}

struct NaiveLockVisitor : public llvm::InstVisitor<NaiveLockVisitor> {
  llvm::Function &F;
  // map from an LLVM pointer to the associated "result of a lock" (whatever we want to call that)
  std::map<llvm::Value *, llvm::Value *> m_associated_lock;
  NaiveLockVisitor(llvm::Function &F) : F(F) {}

  void setAssociated(llvm::Value *ptr, llvm::Value *tptr) { m_associated_lock[ptr] = tptr; }

  llvm::Value *getAssociated(llvm::Value *ptr) {
		// alaska::println("get ", *ptr);
    if (m_associated_lock[ptr] == NULL) {
      if (auto I = dyn_cast<llvm::Instruction>(ptr)) {
        visit(I);
      } else {
        auto insertPoint = F.front().getFirstNonPHI();
        auto t = wrapped_insert_runtime(alaska::InsertionType::Lock, ptr, insertPoint);
        setAssociated(ptr, t);
      }
    }

    return m_associated_lock[ptr];
  }

  void visitGetElementPtrInst(llvm::GetElementPtrInst &I) {
    // create a new GEP right after this one.
    IRBuilder<> b(I.getNextNode());

    auto t = getAssociated(I.getPointerOperand());
    std::vector<llvm::Value *> inds(I.idx_begin(), I.idx_end());
    setAssociated(&I, b.CreateGEP(I.getSourceElementType(), t, inds, "", I.isInBounds()));
  }

  void visitLoadInst(llvm::LoadInst &I) {
    auto t = getAssociated(I.getPointerOperand());
    I.setOperand(0, t);
  }
  void visitStoreInst(llvm::StoreInst &I) {
    auto t = getAssociated(I.getPointerOperand());
    I.setOperand(1, t);
  }


  void visitPHINode(llvm::PHINode &I) {
    IRBuilder<> b(I.getNextNode());
    auto p = b.CreatePHI(I.getType(), I.getNumIncomingValues());
    // set early, as there could be loops
    setAssociated(&I, p);

    for (unsigned int i = 0; i < I.getNumIncomingValues(); i++) {
      auto v = getAssociated(I.getIncomingValue(i));
      p->addIncoming(v, I.getIncomingBlock(i));
    }
  }

  void visitInstruction(llvm::Instruction &I) {
    alaska::println("dunno how to handle this: ", I);
  }
};

void alaska::insertNaiveFlowBasedTranslations(alaska::PointerFlowGraph &G) {
  auto nodes = G.get_nodes();
  auto &F = G.func();
  NaiveLockVisitor nlv(F);
  for (auto node : nodes) {
    if (node->type == alaska::Source) {
      if (auto I = dyn_cast<llvm::Instruction>(node->value)) {
        llvm::Value *t = NULL;
        t = wrapped_insert_runtime(alaska::InsertionType::Lock, node->value, I->getNextNode());
        nlv.setAssociated(node->value, t);
      } else {
        auto insertPoint = F.front().getFirstNonPHI();
        auto t = wrapped_insert_runtime(alaska::InsertionType::Lock, node->value, insertPoint);
        nlv.setAssociated(node->value, t);
      }
    }
  }

  for (auto node : nodes) {
    // only operate on sinks...
    if (node->type != alaska::Sink) continue;
    auto I = dyn_cast<Instruction>(node->value);

    // Insert the get/unlock.
    // We have to handle load and store seperately, as their operand ordering is different (annoyingly...)
    if (auto *load = dyn_cast<LoadInst>(I)) {
      auto ptr = load->getPointerOperand();
      auto t = nlv.getAssociated(ptr);
      load->setOperand(0, t);
      continue;
    }

    if (auto *store = dyn_cast<StoreInst>(I)) {
      auto ptr = store->getPointerOperand();
      auto t = nlv.getAssociated(ptr);
      store->setOperand(1, t);
      continue;
    }
  }
}
