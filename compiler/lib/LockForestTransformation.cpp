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
    : flow_graph(F), pdt(F), forest(flow_graph, pdt) {
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
				auto &bounds = forest.get_lockbounds(child->lock_id);
        child->incoming_lock = alaska::insertLockBefore(bounds.lockBefore, root->val);

      }
    }

    for (auto &child : root->children) {
      apply(*child);
    }
  }

	// finally, insert unlocks
	for (auto &[id, bounds] : forest.locks) {
		alaska::println(id, " has ", bounds->unlocks.size(), " unlock vals"); 
		for (auto *position : bounds->unlocks) {
			if (auto phi = dyn_cast<PHINode>(position)) {
				position = phi->getParent()->getFirstNonPHI();
			}
			alaska::insertUnlockBefore(position, bounds->pointer);
			alaska::println("   ", *position);
		}
	}

  return true;
}
