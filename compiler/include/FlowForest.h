#pragma once

#include <Graph.h>
#include <memory>
#include <vector>

#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/Value.h"

namespace alaska {
  struct FlowForest {
    struct Node {
      Node *share_lock_with = NULL;
      Node *parent;
      std::vector<std::unique_ptr<Node>> children;
      // which siblings does this node dominate in the cfg
      std::unordered_set<Node *> dominates;
      // which siblings does this node postdominate in the cfg
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

    FlowForest(alaska::PointerFlowGraph &G, llvm::PostDominatorTree &PDT);
    std::vector<std::unique_ptr<Node>> roots;
    void dump_dot(void);

   private:
    llvm::Function &func;
  };
};  // namespace alaska
