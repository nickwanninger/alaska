#include <alaska/Translations.h>
#include <alaska/TranslationForest.h>
#include <alaska/PointerFlowGraph.h>
#include <alaska/Utils.h>

#include "llvm/IR/CFG.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/ADT/SparseBitVector.h"

#include <set>

llvm::Value *alaska::Translation::getHandle(void) {
  // the pointer for this translation is simply the first argument of the call to `translate`
  return translation->getOperand(0);
}

llvm::Value *alaska::Translation::getPointer(void) {
  // the handle is the return value of the call to `translate`
  return translation;
}

llvm::Function *alaska::Translation::getFunction(void) {
  if (translation == NULL) return NULL;
  return translation->getFunction();
}

// void alaska::Translation::computeLiveness(llvm::DominatorTree &DT, llvm::PostDominatorTree &PDT)
// {
//   liveBlocks.insert(translation->getParent());
//   for (auto &rel : releases) {
//     liveBlocks.insert(rel->getParent());
//   }
//
//   for (auto &BB : *translation->getFunction()) {
//     bool valid = true;
//     if (!DT.dominates(translation, &BB)) continue;
//
//     for (auto &rel : releases) {
//       if (!PDT.dominates(rel, &BB.front())) {
//         valid = false;
//         break;
//       }
//     }
//
//     if (!valid) continue;
//     liveBlocks.insert(&BB);
//   }
// }


void alaska::Translation::remove(void) {
  alaska::println("alaska::Translation::remove() is not really implemented yet. Don't rely on it!");
  auto *handle = getHandle();
  auto *pointer = getPointer();

  pointer->replaceAllUsesWith(handle);

  this->translation = nullptr;
  this->releases.clear();
  this->users.clear();
  this->liveBlocks.clear();
}


bool alaska::Translation::isUser(llvm::Instruction *inst) {
  return users.find(inst) != users.end();
}


bool alaska::Translation::isLive(llvm::Instruction *inst) {
  if (isLive(inst->getParent())) {
    if (inst == translation) {
      return true;
    }

    if (inst->getParent() == translation->getParent()) {
      if (!translation->comesBefore(inst)) {
        return false;
      }
    }

    for (auto &rel : releases) {
      if (inst == rel) return true;
      if (inst->getParent() == rel->getParent()) {
        return inst->comesBefore(rel);
      }
    }
    return true;
  }
  return false;
}


bool alaska::Translation::isLive(llvm::BasicBlock *bb) {
  return liveBlocks.find(bb) != liveBlocks.end();
}

llvm::Value *alaska::Translation::getRootAllocation() {
  llvm::Value *cur = getHandle();

  while (1) {
    if (auto *gep = dyn_cast<GetElementPtrInst>(cur)) {
      cur = gep->getPointerOperand();
    } else {
      break;
    }
  }

  // If this is an alaska.root, unpack it.
  if (auto *call = dyn_cast<llvm::CallInst>(cur)) {
    if (auto called_function = call->getCalledFunction()) {
      if (called_function->getName() == "alaska.root") {
        return call->getArgOperand(0);
      }
    }
  }

  return cur;
}

static void extract_users(llvm::Instruction *inst, std::set<llvm::Instruction *> &users) {
  for (auto user : inst->users()) {
    if (auto *ui = dyn_cast<Instruction>(user)) {
      if (users.find(ui) == users.end()) {
        users.insert(ui);

        if (auto load = dyn_cast<LoadInst>(ui)) {
          continue;
        }
        extract_users(ui, users);
      }
    }
  }
}


std::vector<std::unique_ptr<alaska::Translation>> alaska::extractTranslations(llvm::Function &F) {
  // We need dominator trees to recalculate the "live blocks"
  llvm::DominatorTree DT(F);
  llvm::PostDominatorTree PDT(F);

  auto &M = *F.getParent();


  // find the translation and release functions
  auto *translateFunction = M.getFunction("alaska.translate");
  auto *releaseFunction = M.getFunction("alaska.release");
  if (translateFunction == NULL) {
    // if the function doesn't exist, there aren't any translations so early return
    return {};
  }

  int next_id = 0;
  std::vector<std::unique_ptr<alaska::Translation>> trs;
  std::unordered_map<int, alaska::Translation *> id_map;



  for (auto &I : instructions(F)) {
    if (auto inst = dyn_cast<CallInst>(&I)) {
      if (inst->getFunction() == &F && inst->getCalledFunction() == translateFunction) {
        auto tr = std::make_unique<alaska::Translation>();
        tr->translation = inst;
        tr->id = next_id++;
        trs.push_back(std::move(tr));
      }
    }
  }

  // associate releases
  if (releaseFunction != NULL) {
    for (auto &I : instructions(F)) {
      if (auto inst = dyn_cast<CallInst>(&I)) {
        if (inst->getFunction() != &F) continue;
        if (inst->getCalledFunction() != releaseFunction) continue;
        for (auto &tr : trs) {
          if (tr->getHandle() == inst->getOperand(0)) {
            tr->releases.insert(inst);
          }
        }
      }
    }
  }

  for (auto &tr : trs) {
    id_map[tr->id] = tr.get();
    extract_users(tr->translation, tr->users);
  }

  // Now, we run a simple basic-block level liveness analysis to populate the
  // `liveBlocks` field in each translation.


  std::unordered_map<BasicBlock *, llvm::SparseBitVector<128>> GEN;
  std::unordered_map<BasicBlock *, llvm::SparseBitVector<128>> KILL;
  std::unordered_map<BasicBlock *, llvm::SparseBitVector<128>> IN;
  std::unordered_map<BasicBlock *, llvm::SparseBitVector<128>> OUT;

  // Populate the GEN/KILL sets for each translation
  for (auto &tr : trs) {
    BasicBlock *bb;
    // KILL = "translations that are created here"
    //      ... Little confusing cause it's a backwards analysis.
    bb = tr->translation->getParent();
    KILL[bb].set(tr->id);
    tr->liveBlocks.insert(bb);

    // GEN = "Translations that are released here"
    for (auto *rel : tr->releases) {
      bb = rel->getParent();
      GEN[bb].set(tr->id);
      tr->liveBlocks.insert(bb);
    }
  }

  // TODO: this should be a unique queue of some kind, really.
  std::unordered_set<BasicBlock *> worklist;

  // populate worklist
  for (auto &BB : F) {
    worklist.insert(&BB);
  }

  while (not worklist.empty()) {
    // grab an entry off the worklist
    auto *bb = *worklist.begin();
    worklist.erase(bb);

    auto old_out = OUT[bb];
    auto old_in = IN[bb];

    IN[bb] = (GEN[bb] | OUT[bb]) - KILL[bb];

    OUT[bb].clear();

    for (auto *s : successors(bb)) {
      OUT[bb] |= IN[s];
    }


    if (old_out != OUT[bb] || old_in != IN[bb]) {
      worklist.insert(bb);
      for (auto *p : predecessors(bb)) {
        worklist.insert(p);
      }
    }
  }

  // Copy the results into each translation.
  for (auto &[bb, live] : IN) {
    for (auto id : live) {
      id_map[id]->liveBlocks.insert(bb);
    }
  }

  return trs;
}




bool alaska::shouldTranslate(llvm::Value *val) {
  if (!val->getType()->isPointerTy()) return false;
  if (dyn_cast<PoisonValue>(val)) return false;
  if (dyn_cast<GlobalValue>(val)) return false;
  if (dyn_cast<AllocaInst>(val)) return false;
  if (dyn_cast<ConstantPointerNull>(val)) return false;

  if (auto call = dyn_cast<CallInst>(val)) {
    auto *func = call->getCalledFunction();
    if (func && func->getName() == "_Znam") return false;
  }

  if (auto arg = dyn_cast<Argument>(val)) {
    auto *func = arg->getParent();
    if (func && func->getName().startswith(".omp_outlined")) {
      return false;
    }
  }

  if (auto gep = dyn_cast<GetElementPtrInst>(val)) {
    return shouldTranslate(gep->getPointerOperand());
  }

  return true;
}


static std::string escape(Instruction &I) {
  std::string raw;
  llvm::raw_string_ostream raw_writer(raw);
  I.print(raw_writer);

  std::string out;

  for (auto c : raw) {
    switch (c) {
      case '<':
        out += "&lt;";
        break;
      case '>':
        out += "&gt;";
        break;
      default:
        out += c;
        break;
    }
  }

  return out;
}


static std::vector<const char *> graphviz_colors = {
    "green",
    "orange",
    "yellow",
    "aqua",
    "chartreuse",
    "crimson",
    "dorkorchid1",
    "darksalmon",
};


static std::string random_background_color() {
  char buf[16];
  uint8_t r = rand() | 0x80;
  uint8_t g = rand() | 0x80;
  uint8_t b = rand() | 0x80;
  snprintf(buf, 15, "#%02x%02x%02x", r & 0xf0, g & 0xf0, b & 0xf0);
  return buf;
}

void alaska::printTranslationDot(llvm::Function &F,
    std::vector<std::unique_ptr<alaska::Translation>> &trs, llvm::raw_ostream &out) {
  std::map<alaska::Translation *, std::string> colors;

  for (size_t i = 0; i < trs.size(); i++) {
    colors[trs[i].get()] = random_background_color();
  }


  alaska::fprintln(out, "digraph {");
  alaska::fprintln(out, "  label=\"translations in ", F.getName(), "\";");
  alaska::fprintln(out, "  compound=true;");
  alaska::fprintln(out, "  graph [fontsize=8];");  // splines=\"ortho\"];");
  alaska::fprintln(out, "  node[fontsize=9,shape=none,style=filled,fillcolor=white];");


  for (auto &BB : F) {
    alaska::fprintln(out, "  n", &BB, " [label=<");
    alaska::fprintln(out, "    <table border=\"1\" cellspacing=\"0\" padding=\"0\">");

    // generate the label
    alaska::fprint(out, "      <tr><td align=\"left\" port=\"header\" border=\"0\">");
    BB.printAsOperand(errs(), false);
    alaska::fprint(out, ":</td>");
    alaska::fprintln(out, "</tr>");

    for (auto &I : BB) {
      std::string color = "white";

      for (auto &[tr, lcolor] : colors) {
        if (tr->translation == &I) {
          color = lcolor;
          break;
        }
        for (auto &rel : tr->releases) {
          if (rel == &I) {
            color = lcolor;
          }
        }
      }

      if (auto call = dyn_cast<CallInst>(&I)) {
        if (auto f = call->getCalledFunction()) {
          if (f->getName() == "alaska_barrier_poll") {
            color = "#ff00ff";
          }
        }
      }
      alaska::fprint(out, "      <tr><td align=\"left\" port=\"n", &I, "\" border=\"0\" bgcolor=\"",
          color, "\">  ", escape(I), "</td>");


      for (auto &[tr, color] : colors) {
        if (tr->isLive(&I) || &I == tr->translation ||
            tr->releases.find(dyn_cast<CallInst>(&I)) != tr->releases.end()) {
          alaska::fprint(out, "<td align=\"left\" border=\"0\" bgcolor=\"", color, "\"> ");
          tr->getHandle()->printAsOperand(out);
          alaska::fprint(out, " </td>");
        }
      }

      alaska::fprintln(out, "</tr>");
    }
    alaska::fprintln(out, "    </table>>];");  // end label
  }


  for (auto &BB : F) {
    auto *term = BB.getTerminator();
    for (unsigned i = 0; i < term->getNumSuccessors(); i++) {
      auto *other = term->getSuccessor(i);
      alaska::fprintln(out, "  n", &BB, ":n", term, " -> n", other, ":header");
    }
  }
  alaska::fprintln(out, "}");
}




void alaska::insertHoistedTranslations(llvm::Function &F) {
  alaska::TranslationForest forest(F);
  (void)forest.apply();
}




void alaska::insertConservativeTranslations(llvm::Function &F) {
  alaska::PointerFlowGraph G(F);

  for (auto &BB : F) {
    for (auto &I : BB) {
      if (auto *load = dyn_cast<LoadInst>(&I)) {
        auto ptr = load->getPointerOperand();
        if (!shouldTranslate(ptr)) continue;
        auto t = insertTranslationBefore(&I, ptr);
        load->setOperand(0, t);
        alaska::insertReleaseBefore(I.getNextNode(), ptr);
        continue;
      }

      if (auto *store = dyn_cast<StoreInst>(&I)) {
        auto ptr = store->getPointerOperand();
        if (!shouldTranslate(ptr)) continue;
        auto t = insertTranslationBefore(&I, ptr);
        store->setOperand(1, t);
        insertReleaseBefore(I.getNextNode(), ptr);
        continue;
      }
    }
  }

  // // Naively insert translate/release around loads and stores (the sinks in the graph provided)
  // auto nodes = G.get_nodes();
  // // Loop over all the nodes...
  // for (auto node : nodes) {
  //   // only operate on sinks...
  //   if (node->type != alaska::Sink) continue;
  //   auto inst = dyn_cast<Instruction>(node->value);

  //   auto dbg = inst->getDebugLoc();
  //   // Insert the translate/release.
  //   // We have to handle load and store seperately, as their operand ordering is different
  //   // (annoyingly...)
  //   if (auto *load = dyn_cast<LoadInst>(inst)) {
  //     auto ptr = load->getPointerOperand();
  //     auto t = insertTranslationBefore(inst, ptr);
  //     load->setOperand(0, t);
  //     alaska::insertReleaseBefore(inst->getNextNode(), ptr);
  //     continue;
  //   }

  //   if (auto *store = dyn_cast<StoreInst>(inst)) {
  //     auto ptr = store->getPointerOperand();
  //     auto t = insertTranslationBefore(inst, ptr);
  //     store->setOperand(1, t);
  //     insertReleaseBefore(inst->getNextNode(), ptr);
  //     continue;
  //   }
  // }
}
