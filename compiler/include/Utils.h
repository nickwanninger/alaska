#include "llvm/IR/Instruction.h"

#include <Graph.h>

namespace alaska {
  enum InsertionType { Pin, UnPin };

  // Split the basic block before the instruction and insert a guarded
  // call to pin or unpin, based on if the `handle` is a handle or not
  llvm::Value *insertGuardedRTCall(InsertionType type, llvm::Value *handle, llvm::Instruction *inst);


  // Insert pins for a graph conservatively (every load and store)
  void insertConservativePins(alaska::PinGraph &G);
}  // namespace alaska