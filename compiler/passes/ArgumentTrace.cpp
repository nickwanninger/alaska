#include <alaska/Passes.h>
#include <alaska/Translations.h>
#include <alaska/Utils.h>

using namespace llvm;

PreservedAnalyses AlaskaArgumentTracePass::run(Module &M, ModuleAnalysisManager &AM) {
  auto &ctx = M.getContext();
  auto ptrType = PointerType::get(ctx, 0);
  auto i64Ty = IntegerType::get(M.getContext(), 64);
  auto trackFunctionType = FunctionType::get(ptrType, {ptrType, i64Ty}, true);
  std::string name = "alaska_argtrack";
  auto trackFunction = M.getOrInsertFunction(name, trackFunctionType).getCallee();
  auto nullConstant = ConstantPointerNull::get(ptrType);
  for (auto &F : M) {
    if (F.empty()) continue;

    if (F.getName().startswith("alaska.")) continue;
    if (F.getName().startswith("alaska_")) continue;
    if (F.getName().startswith("__alaska")) continue;

    IRBuilder<> atEntry(F.front().getFirstNonPHI());


    llvm::Constant *nameString = llvm::ConstantDataArray::getString(ctx, F.getName(), true);
    llvm::GlobalVariable *nameGlobal =
        new llvm::GlobalVariable(M,             // Module to add the global variable to
            nameString->getType(),              // Type of the constant string
            true,                               // Constant global variable
            llvm::GlobalValue::PrivateLinkage,  // Linkage type
            nameString,                         // The actual string constant
            "ARGTRACK_" + F.getName()           // Name of the global variable
        );

    std::vector<Value *> args;
    args.push_back(nameGlobal);
    args.push_back(ConstantInt::get(i64Ty, F.arg_size()));

    for (auto &arg : F.args()) {
      if (arg.getType()->isPointerTy()) {
        args.push_back(&arg);
      } else {
        args.push_back(nullConstant);
      }
    }


    atEntry.CreateCall(trackFunctionType, trackFunction,
        args);  // {nameGlobal, ConstantInt::get(Type::getInt32Ty(ctx), 0)});
  }
  return PreservedAnalyses::all();
}
