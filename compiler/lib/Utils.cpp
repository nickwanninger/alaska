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


static bool is_tracing(void) {
  return getenv("ALASKA_COMPILER_TRACE") != NULL;
}
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

llvm::Instruction *alaska::insertTranslationBefore(llvm::Instruction *inst, llvm::Value *pointer) {
  auto *headBB = inst->getParent();
  auto &F = *headBB->getParent();
  auto &M = *F.getParent();
  LLVMContext &ctx = M.getContext();
  auto ptrType = PointerType::get(ctx, 0);
  auto translateFunctionType = FunctionType::get(ptrType, {ptrType}, false);
  std::string name = "alaska_translate";
  if (is_tracing()) name += "_trace";
  if (bootstrapping()) name = "alaska_translate_bootstrap";
  auto translateFunction = M.getOrInsertFunction(name, translateFunctionType).getCallee();

  IRBuilder<> b(inst);
  b.SetCurrentDebugLocation(inst->getDebugLoc());
  auto translated = b.CreateCall(translateFunctionType, translateFunction, {pointer});
  // translated->setDebugLoc(DILocation::get(ctx, 0, 0,
  // inst->getFunction()->getSubprogram()->getScope()));
  translated->setDebugLoc(getFirstDILocationInFunctionKillMe(inst->getFunction()));
  if (!translated->getDebugLoc()) {
  }
  return translated;
}

llvm::Instruction *alaska::insertReleaseBefore(llvm::Instruction *inst, llvm::Value *pointer) {
  auto *headBB = inst->getParent();
  auto &F = *headBB->getParent();
  auto &M = *F.getParent();
  LLVMContext &ctx = M.getContext();
  auto ptrType = PointerType::get(ctx, 0);
  auto ftype = FunctionType::get(Type::getVoidTy(ctx), {ptrType}, false);

  std::string name = "alaska_release";
  if (is_tracing()) name += "_trace";
  if (bootstrapping()) name = "alaska_release_bootstrap";
  auto func = M.getOrInsertFunction(name, ftype).getCallee();

  IRBuilder<> b(inst);
  b.SetCurrentDebugLocation(inst->getDebugLoc());
  auto release = b.CreateCall(ftype, func, {pointer});
  release->setDebugLoc(getFirstDILocationInFunctionKillMe(inst->getFunction()));

  return release;
}
