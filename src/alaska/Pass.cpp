#include "./PinAnalysis.h"
#include "llvm/IR/Operator.h"

#define ADDR_SPACE 0

namespace {
  struct AlaskaPass : public ModulePass {
    static char ID;
    llvm::Type *int64Type;

    AlaskaPass() : ModulePass(ID) {
    }

    bool doInitialization(Module &M) override {
      return false;
    }

    bool runOnModule(Module &M) override {
      if (M.getNamedMetadata("alaska") != nullptr) {
        return false;
      }
      M.getOrInsertNamedMetadata("alaska");
      LLVMContext &ctx = M.getContext();
      int64Type = Type::getInt64Ty(ctx);
      auto ptrType = PointerType::get(ctx, 0);

      auto pinFunctionType = FunctionType::get(ptrType, {ptrType}, false);
      auto pinFunction = M.getOrInsertFunction("alaska_pin", pinFunctionType).getCallee();
      // auto pinFunctionFunc = dyn_cast<Function>(pinFunction);

      auto unpinFunctionType = FunctionType::get(Type::getVoidTy(ctx), {ptrType}, false);
      auto unpinFunction = M.getOrInsertFunction("alaska_unpin", unpinFunctionType).getCallee();
      // auto unpinFunctionFunc = dyn_cast<Function>(unpinFunction);

      long fran = 0;
      long fcount = 0;
      for (auto &F : M) {
        (void)F;
        fcount += 1;
      }


      // The instructions where a the value used must be pinned.
      std::vector<llvm::Instruction *> accessInstructions;


      // ====================================================================
      for (auto &F : M) {
        long inserted = 0;
        fran += 1;
        if (F.empty()) continue;


        auto section = F.getSection();
        if (section.startswith("$__ALASKA__")) {
          F.setSection("");
          continue;
        }

        auto maybeTranslate = [&](llvm::Instruction &I, llvm::Value *toTranslate) -> llvm::Value * {
          if (dyn_cast<AllocaInst>(toTranslate) != NULL) return NULL;
          // auto &ctx = I.getContext();
          IRBuilder<> builder(&I);
          // Use the GEP hack to get the size of the access we are guarding
          std::vector<Value *> args;
          args.push_back(toTranslate);
          auto pinned = builder.CreateCall(pinFunctionType, pinFunction, args);
          // errs() << "         I " << I << "\n";
          // errs() << "        do " << *toTranslate << "\n";
          // errs() << "translated " << *pinned << "\n\n";
          // ... insert the unpin as well
          //
          builder.SetInsertPoint(I.getNextNonDebugInstruction());
          builder.CreateCall(unpinFunctionType, unpinFunction, args);
          inserted++;

          return pinned;
        };

        for (auto &BB : F) {
          for (auto &I : BB) {
            if (auto *load = dyn_cast<LoadInst>(&I)) {
              auto toTranslate = load->getPointerOperand();
              auto translated = maybeTranslate(I, toTranslate);
              if (translated) {
                load->setOperand(0, translated);
              }
            } else if (auto *store = dyn_cast<StoreInst>(&I)) {
              auto toTranslate = store->getPointerOperand();
              auto translated = maybeTranslate(I, toTranslate);
              if (translated) {
                store->setOperand(1, translated);
              }
            }
          }
        }

        fprintf(stderr, "%4ld/%-4ld |  %4ld  | %s\n", fran, fcount, inserted, F.getName().data());
      }

      errs() << M << "\n";
      return false;
    }
  };  // namespace
      //
  static RegisterPass<AlaskaPass> X("alaska", "Alaska Pinning", false /* Only looks at CFG */, false /* Analysis Pass */);

  char AlaskaPass::ID = 0;
  // static RegisterPass<AlaskaPass> X("Alaska", "Handle based memory with Alaska");

  static AlaskaPass *_PassMaker = NULL;
  static RegisterStandardPasses _RegPass1(
      PassManagerBuilder::EP_OptimizerLast, [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
        if (!_PassMaker) {
          PM.add(_PassMaker = new AlaskaPass());
        }
      });  // ** for -Ox
  static RegisterStandardPasses _RegPass2(
      PassManagerBuilder::EP_EnabledOnOptLevel0, [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
        if (!_PassMaker) {
          PM.add(_PassMaker = new AlaskaPass());
        }
      });  // ** for -O0
}  // namespace
