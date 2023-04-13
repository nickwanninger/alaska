#pragma once
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"


class LockPrinterPass : public llvm::PassInfoMixin<LockPrinterPass> {
 public:
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &AM);
};


class RedundantArgumentLockElisionPass : public llvm::PassInfoMixin<RedundantArgumentLockElisionPass> {
 public:
  llvm::Value *getRootAllocation(llvm::Value *cur);
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &AM);
};


class LockTrackerPass : public llvm::PassInfoMixin<LockTrackerPass> {
 public:
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &AM);
};

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

class AlaskaTranslatePass : public llvm::PassInfoMixin<AlaskaTranslatePass> {
 public:
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &AM);
};

class AlaskaLinkLibraryPass : public llvm::PassInfoMixin<AlaskaLinkLibraryPass> {
 public:
  const char *lib_path = NULL;
  llvm::GlobalValue::LinkageTypes linkage;
  AlaskaLinkLibraryPass(
      const char *lib_path, llvm::GlobalValue::LinkageTypes linkage = llvm::GlobalValue::WeakAnyLinkage);
  void prepareLibrary(llvm::Module &M);
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &AM);
};