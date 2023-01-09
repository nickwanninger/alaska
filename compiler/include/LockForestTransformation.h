#pragma once

#include "Graph.h"
#include "llvm/Analysis/PostDominators.h"
#include <LockForest.h>

namespace alaska {
  class LockForestTransformation {
    alaska::PointerFlowGraph flow_graph;
    llvm::PostDominatorTree pdt;
    alaska::LockForest forest;

   public:
    LockForestTransformation(llvm::Function &F);

    bool apply(void);

   private:
    bool apply(alaska::LockForest::Node &);
  };
}  // namespace alaska
