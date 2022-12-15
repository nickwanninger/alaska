#pragma once

#include "Graph.h"
#include "llvm/Analysis/PostDominators.h"
#include <FlowForest.h>

namespace alaska {
  class FlowForestTransformation {
    alaska::PointerFlowGraph flow_graph;
    llvm::PostDominatorTree pdt;
    alaska::FlowForest forest;

   public:
    FlowForestTransformation(llvm::Function &F);

    bool apply(void);

   private:
    bool apply(alaska::FlowForest::Node &);
  };
}  // namespace alaska