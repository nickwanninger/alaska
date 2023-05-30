#pragma once
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"




/**
 * AlaskaTranslatePass - As the core pass in alaska, this pass finds all potential
 * pointers in the program that are victims of load/store operations and ensures
 * that they are first translated if they need to be.
 *
 * It achieves this by walking up the use-def chains of load/store operands to find
 * the 'root allocations' in a function. It then inserts translate runtime calls in
 * a hoisted manner (as close to 'allocation site' as possible, outside of loops)
 * and inserts release runtime calls when those allocations 'die' in a liveness
 * analysis.
 */
class AlaskaTranslatePass : public llvm::PassInfoMixin<AlaskaTranslatePass> {
 public:
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &AM);
};


/**
 * LockInsertionPass - Insert lock roots on the stack so the runtime knows where
 * all active handles are. In a runtime that is able to move handles, it must be
 * aware of all the handles that *cannot* be moved as they are currently in use.
 *
 * The way this is done is via a standard method seen in almost every garbage
 * collected system. Effectively, the compiler inserts an allocation on each
 * call stack frame that uses handles. This allocations is simply a node in a
 * 'shadow stack' with a well defined structure that contains each handle that
 * is currently in use. When a translation is performed, this pass will track
 * that handle in the 'shadow stack'.
 *
 * This allows each thread to track their active handles *privately* without
 * communicating with other threads until they need to.
 */
class LockInsertionPass : public llvm::PassInfoMixin<LockInsertionPass> {
 public:
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &AM);
};


/**
 * LockPrinterPass - A pass which extracts translation information of each
 * function and prints them to stdout in a .dot format.
 */
class LockPrinterPass : public llvm::PassInfoMixin<LockPrinterPass> {
 public:
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &AM);
};


class RedundantArgumentLockElisionPass
    : public llvm::PassInfoMixin<RedundantArgumentLockElisionPass> {
 public:
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &AM);
};


/**
 * AlaskaNormalizePass - LLVM IR is complicated and has certain constructs that
 * make analysis difficult sometimes. This pass is simple and just lowers many
 * of those constructs into forms that are more easily analyzable.
 */
class AlaskaNormalizePass : public llvm::PassInfoMixin<AlaskaNormalizePass> {
 public:
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &AM);
};

class AlaskaEscapePass : public llvm::PassInfoMixin<AlaskaEscapePass> {
 public:
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &AM);
};

class AlaskaReplacementPass : public llvm::PassInfoMixin<AlaskaReplacementPass> {
 public:
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &AM);
};

class AlaskaLinkLibraryPass : public llvm::PassInfoMixin<AlaskaLinkLibraryPass> {
 public:
  const char *lib_path = NULL;
  llvm::GlobalValue::LinkageTypes linkage;
  AlaskaLinkLibraryPass(const char *lib_path,
      llvm::GlobalValue::LinkageTypes linkage = llvm::GlobalValue::WeakAnyLinkage);
  void prepareLibrary(llvm::Module &M);
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &AM);
};


class AlaskaLowerPass : public llvm::PassInfoMixin<AlaskaLowerPass> {
 public:
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &AM);
};
