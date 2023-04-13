#pragma once

#include <Graph.h>
#include <memory>
#include <vector>
#include <Locks.h>
#include <config.h>

#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/Value.h"

namespace alaska {

  /**
   * LockBounds
   *
   * The result of a lock forest analysis is the locations
   * where to place calls to alaska_get and alaska_put.
   */
  struct LockBounds {
    unsigned id;
    // The value that is being locked
    llvm::Value *pointer;
    // The result of the lock once it has been inserted
    llvm::Instruction *locked;
    // The location to lock at
    llvm::Instruction *lockBefore;
    // The locations to unlock
    std::set<llvm::Instruction *> unlocks;
  };

  struct LockForest {
    struct Node {
      Node *share_lock_with = NULL;
      Node *parent;
      // which of the LockBounds does this node use?
      unsigned lock_id = UINT_MAX;
      std::vector<std::unique_ptr<Node>> children;

      // which siblings does this node dominates and post dominates in the cfg.
      // This is used to share locks among multiple subtrees in the forest.
      std::set<Node *> dominates;
      std::set<Node *> postdominates;


      llvm::Value *val;
      // If this node performs a lock on incoming data, this is where it is located.
      llvm::Instruction *incoming_lock = nullptr;
      llvm::Instruction *translated = nullptr;  // filled in by the flow forest transformation

      Node(alaska::FlowNode *val, Node *parent = nullptr);
      int depth(void);
      Node *compute_shared_lock(void);
      llvm::Instruction *effective_instruction(void);

      void set_lockid(unsigned id) {
        lock_id = id;
        for (auto &c : children)
          c->set_lockid(id);
      }
    };

    LockForest(llvm::Function &F);

    // void apply(void);

		// Insert locks and unlocks, and return the vector of locks that were inserted
		void apply(Node &n);
		std::vector<std::unique_ptr<alaska::Lock>> apply(void);

    std::vector<std::unique_ptr<Node>> roots;
    void dump_dot(void);


    // Nodes in the flow forest are assigned a lock location, which after analysis is completed
    // determine the location of alaska_get and alaska_put calls. They are simply a mapping
    // from
    std::map<unsigned, std::unique_ptr<LockBounds>> locks;

    // get a lockbounds by id
    LockBounds &get_lockbounds(unsigned id);

    llvm::Function &func;

   private:
    // allocate a new lockbounds
    LockBounds &get_lockbounds(void);
    unsigned next_lock_id = 0;
  };

};  // namespace alaska
