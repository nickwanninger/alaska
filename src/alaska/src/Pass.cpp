// #include "analysis/Grpah.h"


#include <Graph.h>
#include "llvm/IR/Operator.h"
#include <unordered_set>
#include "llvm/IR/InstVisitor.h"
#include <cassert>

#include "llvm/Analysis/AliasAnalysis.h"

using namespace llvm;

struct PinVisitor : public llvm::InstVisitor<PinVisitor> {
  alaska::Node &node;
  PinVisitor(alaska::Node &node) : node(node) {}

  void visitLoadInst(LoadInst &I) {}
  void visitStoreInst(StoreInst &I) {}

  void visitGetElementPtrInst(GetElementPtrInst &I) {}
  void visitPHINode(PHINode &I) {}

  void visitInstruction(Instruction &I) { assert(node.type == alaska::Source); }
};



namespace {
  struct AlaskaPass : public ModulePass {
    static char ID;
    llvm::Type *int64Type;

    AlaskaPass() : ModulePass(ID) {}

    bool doInitialization(Module &M) override { return false; }

    llvm::Value *pin(alaska::Node *node) {
      if (node->pinned_value != NULL) return node->pinned_value;

      if (auto I = dyn_cast<llvm::Instruction>(node->value)) {
        PinVisitor pv(*node);
        pv.visit(I);
      } else {
        //
      }
      return node->value;
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


        std::set<llvm::Instruction *> sinks;

        // pointers that are loaded and/or stored to.
        for (auto &BB : F) {
          for (auto &I : BB) {
            // Handle loads and stores differently, as there isn't a base class to handle them the same way
            if (auto *load = dyn_cast<LoadInst>(&I)) {
              sinks.insert(load);
            } else if (auto *store = dyn_cast<StoreInst>(&I)) {
              sinks.insert(store);
            }
          }
        }


        // Get alias analysis information for the function
        // bool idk = false;
        // auto &aaPass = getAnalysis<AAResultsWrapperPass>(F, &idk);
        // auto &aa = aaPass.getAAResults();


        alaska::PinGraph graph(F);

        fprintf(stderr, "%4ld/%-4ld |  %4ld  | %s\n", fran, fcount, inserted, F.getName().data());
        graph.dump_dot();
      }


      // errs() << M << "\n";
      return false;
    }


    void getAnalysisUsage(AnalysisUsage &AU) const override {
      // our pass requires Alias Analysis
      AU.addRequired<AAResultsWrapperPass>();

      // some loop stuffs
      AU.addRequired<AssumptionCacheTracker>();
      AU.addRequired<DominatorTreeWrapperPass>();
      AU.addRequired<LoopInfoWrapperPass>();
      AU.addRequired<ScalarEvolutionWrapperPass>();
      // AU.addRequired<TargetTransformInfoWrapperPass>();
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
