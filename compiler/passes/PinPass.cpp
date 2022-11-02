// #include "analysis/Grpah.h"


#include <Graph.h>
#include <cassert>
#include <unordered_set>

#include "llvm/IR/Operator.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include "noelle/core/MetadataManager.hpp"


using namespace llvm;

namespace {
  struct AlaskaPass : public ModulePass {
    static char ID;
    llvm::Type *int64Type;

    AlaskaPass() : ModulePass(ID) {}

    bool doInitialization(Module &M) override { return false; }

    bool runOnModule(Module &M) override {
      llvm::noelle::MetadataManager mdm(M);

      if (mdm.doesHaveMetadata("alaska")) {
        return false;
      }

      mdm.addMetadata("alaska", "did run");



      LLVMContext &ctx = M.getContext();
      int64Type = Type::getInt64Ty(ctx);
      auto ptrType = PointerType::get(ctx, 0);

      auto pinFunctionType = FunctionType::get(ptrType, {ptrType}, false);
      auto pinFunction = M.getOrInsertFunction("alaska_pin", pinFunctionType).getCallee();

      auto unpinFunctionType = FunctionType::get(Type::getVoidTy(ctx), {ptrType}, false);
      auto unpinFunction = M.getOrInsertFunction("alaska_unpin", unpinFunctionType).getCallee();
      (void)pinFunction;
      (void)unpinFunction;



      // Codegen the pin function
      //
      // extern void *alaska_pin_internal(void *handle);
      // extern void *alaska_pin(void *handle) {
      //     // Check if the top bit is set
      //     if ((long)handle < 0) {
      //         return alaska_pin_internal(handle);
      //     }
      //     return handle;
      // }

      // auto internalPin = M.getOrInsertFunction("__alaska_pin", pinFunctionType);



      // to compute average user ratios
      std::vector<long> source_users;

      for (auto &F : M) {
        if (F.empty()) continue;
        auto section = F.getSection();
        // Skip functions which are annotated with `alaska_rt`
        if (section.startswith("$__ALASKA__")) {
          F.setSection("");
          continue;
        }

        alaska::PinGraph graph(F);
        auto nodes = graph.get_nodes();

        // auto *DT = &getAnalysis<DominatorTreeWrapperPass>(F).getDomTree();
        // auto *PDT = &getAnalysis<PostDominatorTreeWrapperPass>(F).getPostDomTree();
        // graph.dump_dot(*DT, *PDT);

        for (auto node : nodes) {
          if (node->type != alaska::Source) continue;

          long users = 0;

          for (auto o : nodes) {
            if (o->type != alaska::Sink) {
              // continue;
            }
            for (auto color : o->colors) {
              if (color == node->id) users++;
            }
          }

          source_users.push_back(users);
        }


#ifdef ALASKA_CONSERVATIVE
        enum InsertionType { Pin, UnPin };
        auto insertGuardedRTCallBefore = [&](InsertionType it, llvm::Value *handle, llvm::Instruction *inst) -> llvm::Value * {
          // The original basic block
          auto *headBB = inst->getParent();

          llvm::IRBuilder<> b(inst);
          auto sltInst = b.CreateICmpSLT(handle, llvm::ConstantPointerNull::get(ptrType));


          // a pointer to the terminator of the "Then" block
          auto pinTerminator = llvm::SplitBlockAndInsertIfThen(sltInst, inst, false);
          auto termBB = dyn_cast<BranchInst>(pinTerminator)->getSuccessor(0);

          if (it == InsertionType::Pin) {
            b.SetInsertPoint(pinTerminator);
            auto pinned = b.CreateCall(pinFunctionType, pinFunction, {handle});
            // Create a PHI node between `handle` and `pinned`
            b.SetInsertPoint(termBB->getFirstNonPHI());
            auto phiNode = b.CreatePHI(ptrType, 2);

            phiNode->addIncoming(handle, headBB);
            phiNode->addIncoming(pinned, pinned->getParent());

            return phiNode;
          }
          if (it == InsertionType::UnPin) {
            b.SetInsertPoint(pinTerminator);
            b.CreateCall(unpinFunctionType, unpinFunction, {handle});
            return nullptr;
          }

					return NULL;
        };

        for (auto node : nodes) {
          if (node->type != alaska::Sink) continue;
          auto I = dyn_cast<Instruction>(node->value);
          llvm::Value *ptr = NULL;

          if (auto *load = dyn_cast<LoadInst>(I)) {
            ptr = load->getPointerOperand();
            auto pinned = insertGuardedRTCallBefore(InsertionType::Pin, ptr, load);
            load->setOperand(0, pinned);
            insertGuardedRTCallBefore(InsertionType::UnPin, ptr, I->getNextNode());
          } else if (auto *store = dyn_cast<StoreInst>(I)) {
            ptr = store->getPointerOperand();
            auto pinned = insertGuardedRTCallBefore(InsertionType::Pin, ptr, store);
            store->setOperand(1, pinned);
            insertGuardedRTCallBefore(InsertionType::UnPin, ptr, I->getNextNode());
            // b.CreateCall(unpinFunctionType, unpinFunction, {ptr});
          }
        }
        // alaska::println(F);
#endif
      }
      long total = 0;
      for (auto u : source_users)
        total += u;
      printf("average source:sink ratio: %f\n", total / (float)source_users.size());

      // alaska::println(M);

      return true;
    }


    void getAnalysisUsage(AnalysisUsage &AU) const override {
      // our pass requires Alias Analysis
      AU.addRequired<AAResultsWrapperPass>();

      // some loop stuffs
      AU.addRequired<AssumptionCacheTracker>();
      AU.addRequired<DominatorTreeWrapperPass>();
      AU.addRequired<PostDominatorTreeWrapperPass>();
      AU.addRequired<LoopInfoWrapperPass>();
      AU.addRequired<ScalarEvolutionWrapperPass>();
      // AU.addRequired<TargetTransformInfoWrapperPass>();
    }
  };  // namespace
      //
  static RegisterPass<AlaskaPass> X("alaska", "Alaska Pinning", false /* Only looks at CFG */, false /* Analysis Pass */);

  char AlaskaPass::ID = 0;

  // static AlaskaPass *_PassMaker = NULL;
  // static RegisterStandardPasses _RegPass1(
  //     PassManagerBuilder::EP_OptimizerLast, [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
  //       if (!_PassMaker) {
  //         PM.add(_PassMaker = new AlaskaPass());
  //       }
  //     });  // ** for -Ox
  // static RegisterStandardPasses _RegPass2(
  //     PassManagerBuilder::EP_EnabledOnOptLevel0, [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
  //       if (!_PassMaker) {
  //         PM.add(_PassMaker = new AlaskaPass());
  //       }
  //     });

}  // namespace
