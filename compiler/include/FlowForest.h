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
      llvm::Value *translated = nullptr;  // filled in by the flow forest transformation
      Node(alaska::FlowNode *val, Node *parent = nullptr);
      int depth(void);
			Node *compute_shared_lock(void) {
				if (share_lock_with) {
					auto *s = share_lock_with->compute_shared_lock();
					if (s) return s;
					return share_lock_with;
				}
				return nullptr;
			}

      void dump(int d = 0) {
        for (int i = 0; i < d; i++)
          errs() << "    ";
        alaska::println(*val);
        for (auto &c : children)
          c->dump(d + 1);
      }

      llvm::Instruction *effective_instruction(void);
    };

    FlowForest(alaska::PointerFlowGraph &G, llvm::PostDominatorTree &PDT);
    std::vector<std::unique_ptr<Node>> roots;
    void dump_dot(void);

   private:
    llvm::Function &func;
  };
};  // namespace alaska
