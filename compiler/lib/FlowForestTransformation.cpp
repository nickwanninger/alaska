#include <Graph.h>
#include <Utils.h>
#include "llvm/IR/Function.h"
#include <FlowForestTransformation.h>

alaska::FlowForestTransformation::FlowForestTransformation(llvm::Function &F)
    : flow_graph(F), pdt(F), forest(flow_graph, pdt) {
  // everything is done in the constructor initializer list
}


bool alaska::FlowForestTransformation::apply(alaska::FlowForest::Node &node) {
  if (node.translated) return false;

  if (node.parent == NULL) {
    // trivial. Insert call to `alaska_lock`
    auto next = node.effective_instruction()->getNextNode();
    node.translated = alaska::insertGuardedRTCall(alaska::Lock, node.val, next, next->getDebugLoc());
  } else {
    // do something to handle each instruction
    // ...
  }

  // go over children and apply their transformation
  for (auto &child : node.children) {
    apply(*child);
  }
  return true;
}

bool alaska::FlowForestTransformation::apply(void) {
	return false;
  for (auto &root : forest.roots) {
    alaska::println("apply tx to forest:");
    root->dump();
    continue;
    if (!apply(*root)) {
      return false;
    }
  }
  return true;
}
