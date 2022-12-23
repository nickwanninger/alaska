#include <Graph.h>
#include <Utils.h>
#include <FlowForestTransformation.h>
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
llvm::Instruction *get_incoming_translated_value(alaska::FlowForest::Node &node) {
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


alaska::FlowForestTransformation::FlowForestTransformation(llvm::Function &F)
    : flow_graph(F), pdt(F), dt(F), loops(dt), forest(flow_graph, pdt) {
  // everything is done in the constructor initializer list
}





struct TranslationVisitor : public llvm::InstVisitor<TranslationVisitor> {
  alaska::FlowForest::Node &node;
  TranslationVisitor(alaska::FlowForest::Node &node) : node(node) {}


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

llvm::Instruction *alaska::FlowForestTransformation::compute_lock_insertion_location(
    llvm::Value *pointerToLock, llvm::Instruction *lockUser) {
  // the instruction to consider as the "location of the pointer". This is done for things like arguments.
  llvm::Instruction *effectivePointerInstruction = NULL;
  if (auto pointerToLockInst = dyn_cast<llvm::Instruction>(pointerToLock)) {
    effectivePointerInstruction = pointerToLockInst;
  } else if (auto pointerToLockArg = dyn_cast<llvm::Argument>(pointerToLock)) {
    // get the first instruction in the function the argument is a part of;
    effectivePointerInstruction = pointerToLockArg->getParent()->front().getFirstNonPHI();
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
	errs() << "Found loop for " << *pointerToLock << ": " << *targetLoop;
	if (incoming && incoming->getTerminator()) {
		return incoming->getTerminator();
	}

  return lockUser;
}

bool alaska::FlowForestTransformation::apply(alaska::FlowForest::Node &node) {
  if (node.parent == NULL) {
    for (auto &child : node.children) {
      auto *inst = dyn_cast<llvm::Instruction>(child->val);
      if (inst == NULL) {
        errs() << "expected an instruction as a value if there wasn't a parent\n";
        exit(EXIT_FAILURE);
      }
      // first, if the child does not share a lock with anyone, and
      // it's parent is a source, create the incoming lock
      if (child->share_lock_with == NULL && child->parent->parent == NULL) {
        auto lockLocation = compute_lock_insertion_location(node.val, inst);

        // errs() << "Lock:   " << *node.val << "\n";
        // errs() << "User:   " << *inst << "\n";
        // errs() << "Before: " << *lockLocation << "\n";
        // create a lock instruction right before this one, and use it in
        // creating the `translated` value.
        child->incoming_lock =
            insertGuardedRTCall(alaska::InsertionType::Lock, node.val, lockLocation, inst->getDebugLoc());
      }
    }
  } else {
    auto *inst = dyn_cast<llvm::Instruction>(node.val);
    // apply transformation
    TranslationVisitor vis(node);
    vis.visit(inst);
  }

  // go over children and apply their transformation
  for (auto &child : node.children) {
    // go down to the children
    apply(*child);
  }


  return true;
}

bool alaska::FlowForestTransformation::apply(void) {
  for (auto &root : forest.roots) {
    apply(*root);
  }

  return true;
}
