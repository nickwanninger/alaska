#include "llvm/IR/Instruction.h"

#include <Graph.h>

namespace alaska {
  enum InsertionType { Lock, Unlock };

  // Split the basic block before the instruction and insert a guarded
  // call to lock or unlock, based on if the `handle` is a handle or not
  llvm::Value *insertGuardedRTCall(InsertionType type, llvm::Value *handle, llvm::Instruction *inst);
  llvm::Value *insertRTCall(InsertionType type, llvm::Value *handle, llvm::Instruction *inst);


  // Insert get/puts for a graph conservatively (every load and store)
  void insertConservativeTranslations(alaska::PointerFlowGraph &G);
  void insertNaiveFlowBasedTranslations(alaska::PointerFlowGraph &G);
}  // namespace alaska
