#include <Graph.h>
#include <Utils.h>
#include <LockForestTransformation.h>
#include <assert.h>

#include "llvm/IR/Operator.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"


// Either get the `incoming_lock` of this node or the node it shares with, or the parent's translated
llvm::Instruction *get_incoming_translated_value(alaska::LockForest::Node &node) {
  if (node.incoming_lock != NULL) return node.incoming_lock;

  if (node.parent->parent == NULL) {
    if (auto *shared = node.compute_shared_lock()) {
      ALASKA_SANITY(shared->incoming_lock != NULL, "Incoming lock on shared lock owner is null");
      return shared->incoming_lock;
    }
  }

  ALASKA_SANITY(node.parent->translated != NULL, "Parent of node does not have a translated value");
  return node.parent->translated;
}


alaska::LockForestTransformation::LockForestTransformation(llvm::Function &F)
    : flow_graph(F), pdt(F), dt(F), loops(dt), forest(flow_graph, pdt) {
  // everything is done in the constructor initializer list
}




struct TranslationVisitor : public llvm::InstVisitor<TranslationVisitor> {
  alaska::LockForest::Node &node;
  TranslationVisitor(alaska::LockForest::Node &node) : node(node) {}


  void visitGetElementPtrInst(llvm::GetElementPtrInst &I) {
    // create a new GEP right after this one.
    IRBuilder<> b(I.getNextNode());

    auto t = get_incoming_translated_value(node);
    std::vector<llvm::Value *> inds(I.idx_begin(), I.idx_end());
    node.translated = dyn_cast<Instruction>(b.CreateGEP(I.getSourceElementType(), t, inds, "", I.isInBounds()));
  }

  void visitLoadInst(llvm::LoadInst &I) {
    auto t = get_incoming_translated_value(node);
    I.setOperand(0, t);
  }
  void visitStoreInst(llvm::StoreInst &I) {
    auto t = get_incoming_translated_value(node);
    I.setOperand(1, t);
  }

  void visitInstruction(llvm::Instruction &I) {
    alaska::println("dunno how to handle this: ", I);
    ALASKA_SANITY(node.translated != NULL, "Dunno how to handle this node");
  }
};


static llvm::Loop *get_outermost_loop_for_lock(
    llvm::Loop *loopToCheck, llvm::Instruction *pointer, llvm::Instruction *user) {
  // first, check the `loopToCheck` loop. If it fits the requirements we are looking for, early return and don't check
  // subloops. If it doesn't, recurse down to the subloops and see if they fit.
  //
  // 1. if the loop contains the user but not the pointer, this is the best loop.
  //    (we want to lock right before the loop header)
  if (loopToCheck->contains(user) && !loopToCheck->contains(pointer)) {
    return loopToCheck;
  }

  for (auto *subLoop : loopToCheck->getSubLoops()) {
    if (auto *loop = get_outermost_loop_for_lock(subLoop, pointer, user)) {
      return loop;
    }
  }
  return nullptr;
}

llvm::Instruction *alaska::LockForestTransformation::compute_lock_insertion_location(
    llvm::Value *pointerToLock, llvm::Instruction *lockUser) {
  // the instruction to consider as the "location of the pointer". This is done for things like arguments.
  llvm::Instruction *effectivePointerInstruction = NULL;
  if (auto pointerToLockInst = dyn_cast<llvm::Instruction>(pointerToLock)) {
    effectivePointerInstruction = pointerToLockInst;
  } else {
    // get the first instruction in the function the argument is a part of;
    effectivePointerInstruction = lockUser->getParent()->getParent()->front().getFirstNonPHI();
  }
  ALASKA_SANITY(effectivePointerInstruction != NULL, "No effective instruction for pointerToLock");

  llvm::Loop *targetLoop = NULL;
  for (auto loop : loops) {
    targetLoop = get_outermost_loop_for_lock(loop, effectivePointerInstruction, lockUser);
    if (targetLoop != NULL) break;
  }

  // If no loop was found, lock at the lockUser.
  if (targetLoop == NULL) return lockUser;

  llvm::BasicBlock *incoming, *back;
  targetLoop->getIncomingAndBackEdge(incoming, back);
  // errs() << "Found loop for " << *pointerToLock << ": " << *targetLoop;
  if (incoming && incoming->getTerminator()) {
    return incoming->getTerminator();
  }

  return lockUser;
}

bool alaska::LockForestTransformation::apply(alaska::LockForest::Node &node) {
  auto *inst = dyn_cast<llvm::Instruction>(node.val);
  // apply transformation
  TranslationVisitor vis(node);
  vis.visit(inst);

  // go over children and apply their transformation
  for (auto &child : node.children) {
    // go down to the children
    apply(*child);
  }


  return true;
}

bool alaska::LockForestTransformation::apply(void) {
  for (auto &root : forest.roots) {
    for (auto &child : root->children) {
      auto *inst = dyn_cast<llvm::Instruction>(child->val);
      if (inst == NULL) {
        exit(EXIT_FAILURE);
      }
      // first, if the child does not share a lock with anyone, and
      // it's parent is a source, create the incoming lock
      if (child->share_lock_with == NULL && child->parent->parent == NULL) {
        auto *lockLocation = inst;
        lockLocation = compute_lock_insertion_location(root->val, inst);

				child->incoming_lock = alaska::insertLockBefore(lockLocation, root->val);
      }
    }

    for (auto &child : root->children) {
      apply(*child);
    }
  }

  return true;
}
