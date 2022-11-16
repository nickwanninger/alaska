#include "llvm/IR/Instruction.h"

#include <Graph.h>

namespace alaska {
  enum InsertionType { Get, Put };

  // Split the basic block before the instruction and insert a guarded
  // call to get or put, based on if the `handle` is a handle or not
  llvm::Value *insertGuardedRTCall(InsertionType type, llvm::Value *handle, llvm::Instruction *inst);
  llvm::Value *insertRTCall(InsertionType type, llvm::Value *handle, llvm::Instruction *inst);


  // Insert get/puts for a graph conservatively (every load and store)
  void insertConservativeTranslations(alaska::PointerFlowGraph &G);
}  // namespace alaska
