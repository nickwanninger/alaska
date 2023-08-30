#include <alaska/Utils.h>
#include <alaska/WrappedFunctions.h>

#include <execinfo.h>

#include "llvm/IR/Operator.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"



bool alaska::bootstrapping(void) {
  return getenv("ALASKA_BOOTSTRAP") != NULL;
}

struct SimpleFormatVisitor : public llvm::InstVisitor<SimpleFormatVisitor> {
  llvm::raw_string_ostream &out;
  SimpleFormatVisitor(llvm::raw_string_ostream &out)
      : out(out) {
  }

  void visitGetElementPtrInst(llvm::GetElementPtrInst &I) {
    out << "GEP ";
  }

  void visitPHINode(llvm::PHINode &I) {
    out << "PHI ";
  }


  void visitLoadInst(llvm::LoadInst &I) {
    I.printAsOperand(out);
    out << " = load ";
    I.getPointerOperand()->printAsOperand(out);
  }

  void visitStoreInst(llvm::StoreInst &I) {
    out << "store ";
    I.getPointerOperand()->printAsOperand(out);
  }

  void visitInstruction(llvm::Instruction &I) {
    I.print(out);
  }
};


std::string alaska::simpleFormat(llvm::Value *val) {
  std::string str;
  llvm::raw_string_ostream out(str);

  val->print(out);


  return str;
}


// static bool is_tracing(void) {
//   return getenv("ALASKA_COMPILER_TRACE") != NULL;
// }
#define BT_BUF_SIZE 100
void alaska::dumpBacktrace(void) {
  int nptrs;

  void *buffer[BT_BUF_SIZE];
  char **strings;

  nptrs = backtrace(buffer, BT_BUF_SIZE);
  fprintf(stderr, "Backtrace:\n");

  strings = backtrace_symbols(buffer, nptrs);
  if (strings == NULL) {
    perror("backtrace_symbols");
    exit(EXIT_FAILURE);
  }

  for (int j = 0; j < nptrs; j++)
    fprintf(stderr, "\x1b[92m%d\x1b[0m: %s\n", j, strings[j]);

  free(strings);
}


llvm::DILocation *getFirstDILocationInFunctionKillMe(llvm::Function *F) {
  return nullptr;
  for (auto &BB : *F) {
    for (auto &I : BB) {
      if (I.getDebugLoc()) return I.getDebugLoc();
    }
  }
  return NULL;
}



llvm::Instruction *alaska::insertRootBefore(llvm::Instruction *inst, llvm::Value *pointer) {
  auto *headBB = inst->getParent();
  auto &F = *headBB->getParent();
  auto &M = *F.getParent();
  LLVMContext &ctx = M.getContext();
  auto inputType = PointerType::get(ctx, 0);
  auto outputType = PointerType::get(ctx, 0);
  auto translateFunctionType = FunctionType::get(outputType, {inputType}, false);
  std::string name = "alaska.root";
  auto translateFunction = M.getOrInsertFunction(name, translateFunctionType).getCallee();
  IRBuilder<> b(inst);
  return b.CreateCall(translateFunctionType, translateFunction, {pointer});
}

llvm::Instruction *alaska::insertTranslationBefore(llvm::Instruction *inst, llvm::Value *pointer) {
  auto *headBB = inst->getParent();
  auto &F = *headBB->getParent();
  auto &M = *F.getParent();
  LLVMContext &ctx = M.getContext();
  auto fType =
      FunctionType::get(PointerType::get(ctx, 0), {PointerType::get(ctx, 0)}, false);
  std::string name = "alaska.translate";
  auto translateFunction = M.getOrInsertFunction(name, fType).getCallee();
  IRBuilder<> b(inst);
  return b.CreateCall(fType, translateFunction, {pointer});
}

llvm::Instruction *alaska::insertReleaseBefore(llvm::Instruction *inst, llvm::Value *pointer) {
  auto *headBB = inst->getParent();
  auto &F = *headBB->getParent();
  auto &M = *F.getParent();
  LLVMContext &ctx = M.getContext();
  auto ftype = FunctionType::get(Type::getVoidTy(ctx), {PointerType::get(ctx, 0)}, false);

  std::string name = "alaska.release";
  auto func = M.getOrInsertFunction(name, ftype).getCallee();

  IRBuilder<> b(inst);
  return b.CreateCall(ftype, func, {pointer});
}


llvm::Instruction *alaska::insertDerivedBefore(
    llvm::Instruction *inst, llvm::Value *base, llvm::GetElementPtrInst *offset) {
  auto *headBB = inst->getParent();
  auto &F = *headBB->getParent();
  auto &M = *F.getParent();
  LLVMContext &ctx = M.getContext();
  auto ptrType = PointerType::get(ctx, 0);
  auto translateFunctionType = FunctionType::get(ptrType, {ptrType, ptrType}, false);
  std::string name = "alaska.derive";
  auto translateFunction = M.getOrInsertFunction(name, translateFunctionType).getCallee();
  IRBuilder<> b(inst);
  return b.CreateCall(translateFunctionType, translateFunction, {base, offset});
}
