#include "llvm/IR/BuiltinGCs.h"
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/SourceMgr.h>
#include "llvm/Support/InitLLVM.h"
#include <alaska/Utils.h>

#include <iostream>

using namespace llvm;

int main(int argc, char **argv) {
  llvm::InitLLVM X(argc, argv);
  llvm::linkAllBuiltinGCs();

  LLVMContext context;
  SMDiagnostic error;
  auto m = llvm::parseIRFile(argv[1], error, context);
	if (!m) {
		alaska::println("Failed to open input\n");
	}

	ModulePassManager MPM;





  return 0;
}
