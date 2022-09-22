#include "./PinAnalysis.h"
#include "llvm/IR/Operator.h"

#define ADDR_SPACE 0

namespace {
struct AlaskaPass : public ModulePass {
  static char ID;
  llvm::Type *int64Type;
  llvm::Type *voidPtrType;
  llvm::Type *voidPtrTypePinned;
  llvm::Value *translateFunction;
  AlaskaPass() : ModulePass(ID) {}

  bool doInitialization(Module &M) override { return false; }

  bool runOnModule(Module &M) override {
    LLVMContext &ctx = M.getContext();
    voidPtrType = Type::getInt8PtrTy(ctx, 0);
    int64Type = Type::getInt64Ty(ctx);
    voidPtrTypePinned = PointerType::get(Type::getInt8Ty(ctx), ADDR_SPACE);

    auto translateFunctionType =
        FunctionType::get(voidPtrTypePinned, {voidPtrTypePinned}, false);
    translateFunction =
        M.getOrInsertFunction("alaska_pin", translateFunctionType).getCallee();
    auto translateFunctionFunc = dyn_cast<Function>(translateFunction);

    auto unpinFunctionType =
        FunctionType::get(Type::getVoidTy(ctx), {voidPtrTypePinned}, false);
    auto unpinFunction =
        M.getOrInsertFunction("alaska_unpin", unpinFunctionType).getCallee();
    auto unpinFunctionFunc = dyn_cast<Function>(unpinFunction);

		long fran = 0;
		long fcount = 0;
		long inserted = 0;
    for (auto &F : M) {
      if (F.empty())
        continue;
			fcount += 1;
    }

  // ====================================================================
  for (auto &F : M) {
    if (F.empty())
      continue;
		fran += 1;

    auto section = F.getSection();
    if (section.startswith("$__ALASKA__")) {
      F.setSection("");
      continue;
    }

    auto maybeTranslate = [&](llvm::Instruction &I,
                              llvm::Value *toTranslate) -> llvm::Value * {
      if (dyn_cast<AllocaInst>(toTranslate) != NULL)
        return NULL;
      auto &ctx = I.getContext();
      IRBuilder<> builder(&I);
      // Use the GEP hack to get the size of the access we are guarding
      std::vector<Value *> args;
      auto ptr = builder.CreatePointerCast(toTranslate, voidPtrType);
      args.push_back(ptr);
      auto translatedVoidPtr = builder.CreateCall(translateFunction, args);
      auto translated = builder.CreateBitOrPointerCast(translatedVoidPtr,
                                                       toTranslate->getType());
      // errs() << "         I " << I << "\n";
      // errs() << "        do " << *toTranslate << "\n";
      // errs() << "translated " << *translated << "\n\n";
      // ... insert the unpin as well
      //
    	builder.SetInsertPoint(I.getNextNonDebugInstruction());
      builder.CreateCall(unpinFunction, args);
			inserted++;

      return translated;
    };

    for (auto &F : M) {
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
    }

    errs() << fran << "/" << fcount << "  " << inserted << "  " << F.getName() << "\n";
		inserted = 0;;
  }
  return false;
  // ====================================================================

  for (auto &F : M) {
    if (F.empty())
      continue;

    // I think we need to compile LLVM specially to support garbage collection
    // F.setGC("shadow-stack");

    alaska::println("\n\n\n\nrunning on ", F.getName());

    // alaska::println(F);
    std::unordered_map<Value *, Value *> raw_pointers;

    for (auto &BB : F) {
      for (auto &I : BB) {
        // Handle loads and stores differently, as there isn't a base class to
        // handle them the same way
        if (auto *load = dyn_cast<LoadInst>(&I)) {
          auto toTranslate = load->getPointerOperand();
          raw_pointers[toTranslate] = toTranslate;

          if (auto ptype = dyn_cast<llvm::PointerType>(load->getType())) {
            raw_pointers[load] = load;
          }
        } else if (auto *store = dyn_cast<StoreInst>(&I)) {
          auto toTranslate = store->getPointerOperand();
          raw_pointers[toTranslate] = toTranslate;
        }
      }
    }

    bool changed;

    int iterations = 0;
    for (auto &kv : raw_pointers) {
      alaska::println("====================================");
      do {
        iterations++;
        changed = false;

        Value *cur = kv.second;
        alaska::println("start: ", *cur);

        if (auto gep = dyn_cast<GetElementPtrInst>(cur)) {
          alaska::println("gep inst ", *cur);
          cur = gep->getPointerOperand();
        } else if (auto bitcast = dyn_cast<BitCastInst>(cur)) {
          alaska::println("bitcast inst ", *cur);
          cur = bitcast->getOperand(0);
        } else if (auto gepOp = dyn_cast<GEPOperator>(cur)) {
          alaska::println("gep op ", *cur);
          cur = gepOp->getOperand(0);
        } else if (auto bitcastOp = dyn_cast<BitCastOperator>(cur)) {
          alaska::println("bitcast op ", *cur);
          cur = bitcastOp->getOperand(0);
        }
        alaska::println("after: ", *cur, "\n");

        if (cur != kv.second && cur != 0) {
          kv.second = cur;
          changed = true;
        }
      } while (changed);
    }

    std::set<Value *> root_pointers;

    for (auto &kv : raw_pointers) {
      // if `a` is Alloca, don't consider it as a root
      if (auto a = dyn_cast<AllocaInst>(kv.second)) {
        continue;
      }

      if (auto glob = dyn_cast<GlobalVariable>(kv.second)) {
        continue;
      }
      // Don't consider non-pointer instructions as roots
      if (!kv.second->getType()->isPointerTy()) {
        continue;
      }

      root_pointers.insert(kv.second);
    }

    // A map from handles to the relative pin
    std::unordered_map<Value *, Value *> pins;

    alaska::PinAnalysis trace(F);
    // Generate pins for the root pointers
    for (auto *p : root_pointers) {
      trace.add_root(p);
    }

    // Using the roots, compute the trace
    trace.compute_trace();
    trace.inject_pins();
    // alaska::println("after:", F);
  }

  return false;
}
}; // namespace

char AlaskaPass::ID = 0;
static RegisterPass<AlaskaPass> X("Alaska", "Handle based memory with Alaska");

static AlaskaPass *_PassMaker = NULL;
static RegisterStandardPasses
    _RegPass1(PassManagerBuilder::EP_OptimizerLast,
              [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
                if (!_PassMaker) {
                  PM.add(_PassMaker = new AlaskaPass());
                }
              }); // ** for -Ox
static RegisterStandardPasses
    _RegPass2(PassManagerBuilder::EP_EnabledOnOptLevel0,
              [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
                if (!_PassMaker) {
                  PM.add(_PassMaker = new AlaskaPass());
                }
              }); // ** for -O0
} // namespace
