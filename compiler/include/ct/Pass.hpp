#pragma once

// implemented in passes/ComilerTiming.cpp

// LLVM includes
#include "llvm/IR/Constants.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Transforms/Utils/LowerInvoke.h"
#include "llvm/Transforms/Scalar/DCE.h"
#include "llvm/Transforms/IPO/Inliner.h"
#include "llvm/Transforms/IPO/GlobalDCE.h"
#include "llvm/Transforms/IPO/AlwaysInliner.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Transforms/Utils/EscapeEnumerator.h"

class CompilerTimingPass : public llvm::PassInfoMixin<CompilerTimingPass> {
 private:
  void init(llvm::Module &M);

 public:
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &AM);
};