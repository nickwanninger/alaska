#pragma once

#include "Graph.h"
#include "llvm/Analysis/PostDominators.h"
#include <LockForest.h>

namespace alaska {
  class LockForestTransformation {
    alaska::PointerFlowGraph flow_graph;
    llvm::PostDominatorTree pdt;
    llvm::DominatorTree dt;
    llvm::LoopInfo loops;
    alaska::LockForest forest;

   public:
    LockForestTransformation(llvm::Function &F);

    bool apply(void);

   private:

    // Compute the instruction at which a lock should be inserted to minimize the number of runtime invocations, but to
    // also minimize the number of handles which are locked when alaska_barrier functions are called. There are a few
    // decisions it this function makes:
    //   - if @lockUser is inside a loop that @pointerToLock is not:
		//     - If the loop contains a call to `alaska_barrier`, lock before @lockUser
		//     - else, lock before the branch into the loop's header.
		//   - else, lock at @lockUser
    llvm::Instruction *compute_lock_insertion_location(llvm::Value *pointerToLock, llvm::Instruction *lockUser);

    bool apply(alaska::LockForest::Node &);
  };
}  // namespace alaska
