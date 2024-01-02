#pragma once

#include <vector>
#include <memory>
#include <config.h>
#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Instructions.h"

namespace alaska {

  // alaska::Translation: an internal representation of an invocation of alaska_translate,
  // all calls to alaska_release, and all users of said translation.
  struct Translation {

    int id; // Unique ID for this function. Basically meaningless.
    // The beginning of this translation.
    llvm::CallInst *translation;
    // Where does this translation's lifetime end?
    std::set<llvm::CallInst *> releases;
    // *all* users of the translation (even users of geps of the translation)
    std::set<llvm::Instruction *> users;
    // Instructions where this translation is alive. This is computed
    // using a very simple liveness analysis
    std::set<llvm::BasicBlock *> liveBlocks;

    // get the pointer and handle that was translated
    llvm::Value *getPointer(void);
    llvm::Value *getHandle(void);
    // Get the function this translation lives in. Return null if `translation == null`
    llvm::Function *getFunction(void);
    bool isUser(llvm::Instruction *inst);
    bool isLive(llvm::Instruction *inst);
    bool isLive(llvm::BasicBlock *inst);
    // *Fully* remove the translation from the function. This will delete
    // all calls to `translate` and `release`, as well as reverse the usages
    void remove();
    // get the root (static) allocation that this is translating.
    llvm::Value *getRootAllocation(void);


		template<typename F>
			void each_live_inst(F &f) {
				for (auto *bb : liveBlocks) {
					for (auto &I : *bb) {
						if (isLive(&I)) {
							f(I);
						}
					}
				}
      }
  };

  // Return if a certain value should be translated before use. This, simply, checks if the `val` is
  // a global variable or an alloca.
  bool shouldTranslate(llvm::Value *val);

  void insertHoistedTranslations(llvm::Function &F);
  void insertConservativeTranslations(llvm::Function &F);


  // Several useful utility functions
  std::vector<std::unique_ptr<alaska::Translation>> extractTranslations(llvm::Function &F);


  void printTranslationDot(llvm::Function &F,
      std::vector<std::unique_ptr<alaska::Translation>> &trs,
      llvm::raw_ostream &out = llvm::errs());
};  // namespace alaska
