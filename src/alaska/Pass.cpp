#include "./PinAnalysis.h"
#include "llvm/IR/Operator.h"
#include <unordered_set>

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


    // Replace all uses of `handle` with `pinned`, and recursively call
    // for instructions like GEP which transform the pointer.
    void propegate_pin(llvm::Value *handle, llvm::Value *pinned) {
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

      auto unpinFunctionType = FunctionType::get(Type::getVoidTy(ctx), {ptrType}, false);
      auto unpinFunction = M.getOrInsertFunction("alaska_unpin", unpinFunctionType).getCallee();
      // auto unpinFunctionFunc = dyn_cast<Function>(unpinFunction);
      (void)pinFunction;
      (void)unpinFunction;

      long fran = 0;
      long fcount = 0;
      for (auto &F : M) {
        (void)F;
        fcount += 1;
      }


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
        alaska::println(F);


        std::unordered_map<Value *, Value *> raw_pointers;

        // Go through every instruction in the function and get the
        // pointers that are loaded and/or stored to.
        for (auto &BB : F) {
          for (auto &I : BB) {
            // Handle loads and stores differently, as there isn't a base class to handle them the same way
            if (auto *load = dyn_cast<LoadInst>(&I)) {
              auto toTranslate = load->getPointerOperand();
              raw_pointers[toTranslate] = toTranslate;
            } else if (auto *store = dyn_cast<StoreInst>(&I)) {
              auto toTranslate = store->getPointerOperand();
              raw_pointers[toTranslate] = toTranslate;
            }
          }
        }

        for (auto &[p, d] : raw_pointers) {
          alaska::println("interesting pointer: ", *p);
        }
        alaska::println();

        bool changed;

        int iterations = 0;
        for (auto &kv : raw_pointers) {
          do {
            iterations++;
            changed = false;

            Value *cur = kv.second;
            if (auto gep = dyn_cast<GetElementPtrInst>(cur)) {
              cur = gep->getPointerOperand();
            }

            if (auto gepOp = dyn_cast<GEPOperator>(cur)) {
              cur = gepOp->getOperand(0);
            }

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

        for (auto &p : root_pointers) {
          alaska::println("interesting pointer: ", *p);
        }
        alaska::println();

        // Analyze the trace of uses rooted in the set of pointers we reduced to.
        alaska::PinAnalysis trace(F);
        for (auto *p : root_pointers) {
          trace.add_root(p);
        }

        // Using the roots, compute the trace
        trace.compute_trace();
        trace.inject_pins();

        // alaska::println(F);

        fprintf(stderr, "%4ld/%-4ld |  %4ld  | %s\n", fran, fcount, inserted, F.getName().data());
      }

      // errs() << M << "\n";
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
