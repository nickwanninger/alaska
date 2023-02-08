#pragma once

#include <vector>
#include <memory>
#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Instructions.h"

namespace alaska {

  // Lock: an internal representation of an invocation of alaska_lock,
	// all unlocks, and all users of said lock.
  struct Lock {
    llvm::CallInst *lock;
    std::set<llvm::CallInst *> unlocks;
    // *all* users of the lock (even users of geps of the lock)
    std::set<llvm::Instruction *> users;
    // Instructions where this lock is alive. This is computed
		// using a very simple liveness analysis
    std::set<llvm::Instruction *> liveInstructions;

    // get the pointer and handle that was locked
    llvm::Value *getPointer(void);
    llvm::Value *getHandle(void);
		// Get the function this lock lives in. Return null if `lock == null`
		llvm::Function *getFunction(void);
    bool isUser(llvm::Instruction *inst);

		// Compute the liveness for this Lock (populate liveInstructions)
		void computeLiveness(void);

		// *Fully* remove the lock from the function. This will delete
		// all calls to lock and unlock, as well as reverse the usages
		void remove();

		// If `users` is empty, apply the lock to the function to transform
		// all 
		void apply(void);
  };


  void insertHoistedLocks(llvm::Function &F);
  void insertConservativeLocks(llvm::Function &F);



  // Several useful utility functions
  std::vector<std::unique_ptr<alaska::Lock>> extractLocks(llvm::Function &F);
  void computeLockLiveness(llvm::Function &F, std::vector<std::unique_ptr<alaska::Lock>> &locks);
  void computeLockLiveness(llvm::Function &F, std::vector<alaska::Lock *> &locks);


  void printLockDot(llvm::Function &F, std::vector<std::unique_ptr<alaska::Lock>> &locks, llvm::raw_ostream &out = llvm::errs());
};  // namespace alaska
