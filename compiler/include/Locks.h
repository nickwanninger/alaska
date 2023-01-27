#pragma once

#include <vector>
#include <memory>
#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Instructions.h"

namespace alaska {

  // Lock: an internal representation of an invocation of alaska_lock, all unlocks, and all users of said lock.
  struct Lock {
    llvm::CallInst *lock;
    std::set<llvm::CallInst *> unlocks;
    // *all* users of the lock (even users of geps of the lock)
    std::set<llvm::Instruction *> users;

    // get the pointer and handle that was locked
    llvm::Value *getPointer(void);
    llvm::Value *getHandle(void);
    bool isUser(llvm::Instruction *inst);
  };


  void insertHoistedLocks(llvm::Function &F);
  void insertConservativeLocks(llvm::Function &F);

  std::vector<std::unique_ptr<alaska::Lock>> extractLocks(llvm::Function &F);

  void printLockDot(llvm::Function &F, std::vector<std::unique_ptr<alaska::Lock>> &locks);
};  // namespace alaska