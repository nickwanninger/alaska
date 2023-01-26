#pragma once


namespace alaska {

  class LockUser {
  };

  // Lock: an internal representation of an invocation of alaska_lock, all unlocks, and all users of said lock.
  struct Lock {
    llvm::Instruction *lock;
    std::vector<llvm::Instruction *> unlocks;

  };


  std::vector<Lock> extractLocks(llvm::Function &F);
};