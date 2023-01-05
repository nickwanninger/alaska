#pragma once

#include <Graph.h>
#include <memory>
#include <vector>

#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/Value.h"

namespace alaska {

  /**
   * LockBounds
	 *
   * The result of a lock forest analysis is the locations
	 * where to place calls to alaska_lock and alaska_unlock.
   */
  struct LockBounds {
    unsigned id;
    // The value that is being locked
    llvm::Value *pointer;
		// The result of the lock once it has been inserted
		llvm::Value *locked;
    // The location to lock at
    llvm::Instruction *lockBefore;
    // The location to unlock
    llvm::Instruction *unlockBefore;
  };

  struct LockForest {
    struct Node {
      Node *share_lock_with = NULL;
      Node *parent;
      std::vector<std::unique_ptr<Node>> children;

      unsigned lock_id;

      // which siblings does this node dominates and post dominates in the cfg.
      // This is used to share locks among multiple subtrees in the forest.
      std::unordered_set<Node *> dominates;
      std::unordered_set<Node *> postdominates;


      llvm::Value *val;
      // If this node performs a lock on incoming data, this is where it is located.
      llvm::Instruction *incoming_lock = nullptr;
      llvm::Instruction *translated = nullptr;  // filled in by the flow forest transformation
      Node(alaska::FlowNode *val, Node *parent = nullptr);
      int depth(void);
      Node *compute_shared_lock(void);
      llvm::Instruction *effective_instruction(void);
    };

    LockForest(alaska::PointerFlowGraph &G, llvm::PostDominatorTree &PDT);
    std::vector<std::unique_ptr<Node>> roots;
    void dump_dot(void);


    // Nodes in the flow forest are assigned a lock location, which after analysis is completed
    // determine the location of alaska_lock and alaska_unlock calls. They are simply a mapping
    // from
    std::unordered_map<unsigned, LockBounds> locks;

   private:
    unsigned next_lock_id = 0;
    llvm::Function &func;
  };

};  // namespace alaska
