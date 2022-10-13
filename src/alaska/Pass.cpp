#include "./PinAnalysis.h"
#include "llvm/IR/Operator.h"
#include <unordered_set>

namespace {
  struct AlaskaPass : public ModulePass {
    static char ID;
    llvm::Type *int64Type;

    AlaskaPass() : ModulePass(ID) {}

    bool doInitialization(Module &M) override { return false; }


    // Replace all uses of `handle` with `pinned`, and recursively call
    // for instructions like GEP which transform the pointer.
    void propegate_pin(llvm::Value *handle, llvm::Value *pinned) {}

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


        alaska::PinGraph graph(F);
        auto nodes = graph.get_nodes();


        alaska::println("digraph {");
        alaska::println("  beautify=true");
        alaska::println("  concentrate=true");
        for (auto *node : nodes) {
          const char *color = NULL;
          switch (node->type) {
            case alaska::Source:
              color = "red";
              break;
            case alaska::Sink:
              color = "blue";
              break;
            case alaska::Transient:
              color = "black";
              break;
          }

          std::string color_label = "colors:";
          for (auto color : node->colors) {
            color_label += " ";
            color_label += std::to_string(color);
          }
          errs() << "  node" << node->id;
          errs() << "[label=\"" << *node->value << "\\n" << color_label << "\"";
          errs() << ", shape=box";
          errs() << ", color=" << color;
          errs() << "]\n";
          // alaska::println("  node", node->id, "[label=\"", *node->value, "\\n", color_label, "\", shape=\"box\", color=", color, ",
          // xlabel=\"id: ", node->id, "\"];");
        }

        for (auto *node : nodes) {
          for (auto onode : node->get_in_nodes()) {
            alaska::println("  node", onode->id, " -> node", node->id, ";");
          }
        }

        for (auto *node : nodes) {
          for (auto onode : node->get_out_nodes()) {
            alaska::println("  node", node->id, " -> node", onode->id, "[color=red];");
          }
        }
        alaska::println("}");

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
