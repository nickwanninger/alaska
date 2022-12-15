#pragma once

#include <Graph.h>
#include <memory>
#include <vector>

#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/Value.h"

namespace alaska {
  struct FlowForest {
    struct Node {
      Node *parent;
      std::vector<std::unique_ptr<Node>> children;

      llvm::Value *val;
      llvm::Value *translated = nullptr;  // filled in by the flow forest transformation
      Node(alaska::FlowNode *val, Node *parent = nullptr);

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
