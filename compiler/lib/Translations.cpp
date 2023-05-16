#include <alaska/Translations.h>
#include <alaska/TranslationForest.h>
#include <alaska/PointerFlowGraph.h>
#include <alaska/Utils.h>
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include <noelle/core/DataFlow.hpp>

llvm::Value *alaska::Translation::getHandle(void) {
  // the pointer for this translation is simply the first argument of the call to alaska_translate
  return translation->getOperand(0);
}

llvm::Value *alaska::Translation::getPointer(void) {
  // the handle is the return value of the call to alaska_translate
  return translation;
}

llvm::Function *alaska::Translation::getFunction(void) {
  if (translation == NULL) return NULL;
  return translation->getFunction();
}

struct TranslationApplicationVisitor : public llvm::InstVisitor<TranslationApplicationVisitor> {
  alaska::Translation &translation;
  llvm::Value *incoming = NULL;

  TranslationApplicationVisitor(alaska::Translation &tr, llvm::Value *incoming)
      : translation(tr)
      , incoming(incoming) {
  }

  void visitGetElementPtrInst(llvm::GetElementPtrInst &I) {
    // create a new GEP right after this one.
    IRBuilder<> b(I.getNextNode());

    std::vector<llvm::Value *> inds(I.idx_begin(), I.idx_end());
    auto new_incoming = dyn_cast<Instruction>(
        b.CreateGEP(I.getSourceElementType(), incoming, inds, "", I.isInBounds()));

    TranslationApplicationVisitor vis(translation, new_incoming);
    for (auto user : I.users()) {
      vis.visit(dyn_cast<Instruction>(user));
    }
  }

  void visitLoadInst(llvm::LoadInst &I) {
    translation.users.insert(&I);
    I.setOperand(0, incoming);
  }
  void visitStoreInst(llvm::StoreInst &I) {
    translation.users.insert(&I);
    I.setOperand(1, incoming);
  }

  void visitInstruction(llvm::Instruction &I) {
    alaska::println("dunno how to handle this: ", I);
  }
};


void alaska::Translation::computeLiveness(void) {
  if (auto *func = getFunction()) {
    std::vector<alaska::Translation *> lps = {this};
    alaska::computeTranslationLiveness(*func, lps);
  }
}


void alaska::Translation::remove(void) {
  alaska::println("alaska::Translation::remove() is not really implemented yet. Don't rely on it!");
  auto *handle = getHandle();
  auto *pointer = getPointer();

  pointer->replaceAllUsesWith(handle);

  this->translation = nullptr;
  this->releases.clear();
  this->users.clear();
  this->liveInstructions.clear();
}


bool alaska::Translation::isUser(llvm::Instruction *inst) {
  return users.find(inst) != users.end();
}
bool alaska::Translation::isLive(llvm::Instruction *inst) {
  return liveInstructions.find(inst) != liveInstructions.end();
}

llvm::Value *alaska::Translation::getRootAllocation() {
  llvm::Value *cur = getHandle();

  while (1) {
    if (auto gep = dyn_cast<GetElementPtrInst>(cur)) {
      cur = gep->getPointerOperand();
    } else {
      break;
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
  auto &M = *F.getParent();

  // find the translation and release functions
  auto *translateFunction = M.getFunction("alaska_translate");
  auto *releaseFunction = M.getFunction("alaska_release");
  if (translateFunction == NULL) {
    // if the function doesn't exist, there aren't any translations so early return
    return {};
  }

  std::vector<std::unique_ptr<alaska::Translation>> trs;

  // Find all users of translation that are in this function
  for (auto user : translateFunction->users()) {
    if (auto inst = dyn_cast<CallInst>(user)) {
      if (inst->getFunction() == &F) {
        if (inst->getCalledFunction() == translateFunction) {
          auto tr = std::make_unique<alaska::Translation>();
          tr->translation = inst;
          trs.push_back(std::move(tr));
        }
      }
    }
  }

  // associate releases
  if (releaseFunction != NULL) {
    for (auto user : releaseFunction->users()) {
      if (auto inst = dyn_cast<CallInst>(user)) {
        if (inst->getFunction() == &F) {
          if (inst->getCalledFunction() == releaseFunction) {
            for (auto &tr : trs) {
              if (tr->getHandle() == inst->getOperand(0)) {
                tr->releases.insert(inst);
              }
            }
          }
        }
      }
    }
  }

  for (auto &tr : trs) {
    extract_users(tr->translation, tr->users);
  }

  computeTranslationLiveness(F, trs);

  return trs;
}


void alaska::computeTranslationLiveness(
    llvm::Function &F, std::vector<std::unique_ptr<alaska::Translation>> &trs) {
  std::vector<alaska::Translation *> tr_ptrs;

  for (auto &tr : trs)
    tr_ptrs.push_back(tr.get());

  return computeTranslationLiveness(F, tr_ptrs);
}

void alaska::computeTranslationLiveness(
    llvm::Function &F, std::vector<alaska::Translation *> &trs) {
  // mapping from instructions to the translation they use.
  std::map<llvm::Instruction *, std::set<alaska::Translation *>> inst_to_tr;
  // map from the call to alaska_translate to the translation structure it belongs to.
  std::map<llvm::Instruction *, alaska::Translation *> tr_inst_to_tr;

  for (auto &tr : trs) {
    // clear the existing liveness info
    tr->liveInstructions.clear();
    tr_inst_to_tr[tr->translation] = tr;

    for (auto &user : tr->users) {
      inst_to_tr[user].insert(tr);
    }
  }

  auto get_translation_of = [](llvm::Instruction *inst) -> llvm::Value * {
    if (auto call = dyn_cast<CallInst>(inst)) {
      auto func = call->getCalledFunction();
      if (func && func->getName() == "alaska_translate") {
        return call->getArgOperandUse(0);
      }
    }

    return nullptr;
  };

  auto get_release_of = [](llvm::Instruction *inst) -> llvm::Value * {
    if (auto call = dyn_cast<CallInst>(inst)) {
      auto func = call->getCalledFunction();
      if (func && func->getName() == "alaska_release") {
        return call->getArgOperandUse(0);
      }
    }

    return nullptr;
  };


  // gen is every release
  auto computeGEN = [&](Instruction *s, noelle::DataFlowResult *df) {
    if (auto v = get_release_of(s)) {
      df->GEN(s).insert(v);
    }
  };

  // Kill is every translation
  auto computeKILL = [&](Instruction *s, noelle::DataFlowResult *df) {
    if (auto v = get_translation_of(s)) {
      df->KILL(s).insert(v);
    }
  };

  auto computeIN = [&](std::set<Value *> &IN, Instruction *s, noelle::DataFlowResult *df) {
    // IN[s] = GEN[s] U (OUT[s] - KILL[s])
    auto &gen = df->GEN(s);
    auto &out = df->OUT(s);
    auto &kill = df->KILL(s);

    // everything in GEN...
    IN.insert(gen.begin(), gen.end());

    // ...and everything in OUT that isn't in KILL
    for (auto o : out) {
      // if o is not found in KILL...
      if (kill.find(o) == kill.end()) {
        // ...add to IN
        IN.insert(o);
      }
    }
  };

  auto computeOUT = [&](std::set<Value *> &OUT, Instruction *p, noelle::DataFlowResult *df) {
    // OUT[s] = IN[p] where p is every successor of s
    auto &INp = df->IN(p);
    OUT.insert(INp.begin(), INp.end());
  };

  auto *df =
      noelle::DataFlowEngine().applyBackward(&F, computeGEN, computeKILL, computeIN, computeOUT);

  // I don't like this.
  for (auto &[inst, values] : df->IN()) {
    for (auto l : values) {
      for (auto &tr : trs) {
        if (tr->getHandle() == l) {
          tr->liveInstructions.insert(inst);
        }
      }
    }
  }


  delete df;
}



bool alaska::shouldTranslate(llvm::Value *val) {
  if (!val->getType()->isPointerTy()) return false;
  if (dyn_cast<GlobalValue>(val)) return false;
  if (dyn_cast<AllocaInst>(val)) return false;
  if (dyn_cast<ConstantPointerNull>(val)) return false;

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
      alaska::fprint(out, "      <tr><td align=\"left\" port=\"n", &I, "\" border=\"0\" bgcolor=\"",
          color, "\">  ", escape(I), "</td>");


      for (auto &[tr, color] : colors) {
        if (tr->liveInstructions.find(&I) != tr->liveInstructions.end() || &I == tr->translation ||
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




struct PotentialSourceFinder : public llvm::InstVisitor<PotentialSourceFinder> {
  std::set<llvm::Value *> srcs;
  std::set<llvm::Value *> accessed;
  std::set<llvm::Value *> seen;


  // returns true if this has been seen or not
  bool track_seen(llvm::Value &v) {
    if (seen.find(&v) != seen.end()) return true;
    seen.insert(&v);
    return false;
  }

  void add_if_pointer(llvm::Value *v) {
    if (v->getType()->isPointerTy()) {
      srcs.insert(v);
    }
  }

  void look_at(llvm::Value *v) {
    if (track_seen(*v)) return;
    // alaska::println("Look at ", *v);

    if (auto inst = dyn_cast<llvm::Instruction>(v)) {
      visit(*inst);
    } else if (auto global_variable = dyn_cast<llvm::GlobalVariable>(v)) {
      return;  // skip global variables
    } else {
      add_if_pointer(v);
    }
  }

  void visitGetElementPtrInst(llvm::GetElementPtrInst &I) {
    look_at(I.getPointerOperand());
  }

  void visitLoadInst(llvm::LoadInst &I) {
    add_if_pointer(&I);
    accessed.insert(I.getPointerOperand());
    look_at(I.getPointerOperand());
  }

  void visitStoreInst(llvm::StoreInst &I) {
    // Simple
    accessed.insert(I.getPointerOperand());
    look_at(I.getPointerOperand());
  }

  void visitPHINode(llvm::PHINode &I) {
    for (auto &incoming : I.incoming_values()) {
      look_at(incoming);
    }
  }

  void visitAllocaInst(llvm::AllocaInst &I) {
    // Nothing. Don't translate allocas
  }

  void visitCallInst(llvm::CallInst &I) {
    auto translateFunction = I.getFunction()->getParent()->getFunction("alaska_translate");
    if (translateFunction != NULL && I.getCalledFunction() == translateFunction) {
      return;
    }
    add_if_pointer(&I);
  }

  void visitInstruction(llvm::Instruction &I) {
    add_if_pointer(&I);
  }
};



struct TransientMappingVisitor : public llvm::InstVisitor<TransientMappingVisitor> {
  std::set<llvm::Instruction *> transients;
  std::set<llvm::Value *> seen;

  // returns true if this has been seen or not
  bool track_seen(llvm::Value &v) {
    if (seen.find(&v) != seen.end()) return true;
    seen.insert(&v);
    return false;
  }

  void look_at(llvm::Value *v) {
    if (track_seen(*v)) return;
    //
    if (auto inst = dyn_cast<llvm::Instruction>(v)) {
      visit(*inst);
    }

    for (auto user : v->users()) {
      look_at(user);
    }
  }

  void visitGetElementPtrInst(llvm::GetElementPtrInst &I) {
    transients.insert(&I);
  }

  void visitPHINode(llvm::PHINode &I) {
    transients.insert(&I);
  }
};


void alaska::insertHoistedTranslations(llvm::Function &F) {
  alaska::TranslationForest forest(F);
  (void)forest.apply();
  return;
  // find all potential sources:
  PotentialSourceFinder s;

  std::vector<llvm::Instruction *> insts;

  for (auto &BB : F)
    for (auto &I : BB)
      insts.push_back(&I);

  for (auto *inst : insts)
    s.look_at(inst);

  std::vector<std::unique_ptr<alaska::Translation>> translations;
  std::map<llvm::Value *, alaska::Translation *> value_to_translation;

  std::map<llvm::Value *, std::set<llvm::Value *>> sourceAliases;

  auto &sources = s.srcs;
  alaska::println("Potential sources for ", F.getName(), ":");
  for (auto &src : sources) {
    // If the source was not accessed, ignore it.
    // if (s.accessed.find(src) == s.accessed.end()) {
    //   continue;
    // }
    sourceAliases[src].insert(src);
    alaska::println("  - ", simpleFormat(src));
    TransientMappingVisitor tv;
    tv.look_at(src);
    alaska::println("    Transients:");
    for (auto &t : tv.transients) {
      sourceAliases[t].insert(src);
      alaska::println("       - ", simpleFormat(t));
    }
  }


  alaska::println("Source Aliases:");
  for (auto &[a, srcs] : sourceAliases) {
    alaska::println(*a);
    for (auto &src : srcs) {
      alaska::println("    - ", simpleFormat(src));
    }
  }


  return;

  auto get_used = [&](llvm::Instruction *s) -> std::set<llvm::Value *> {
    if (auto load = dyn_cast<LoadInst>(s)) return sourceAliases[load->getPointerOperand()];
    if (auto store = dyn_cast<StoreInst>(s)) return sourceAliases[store->getPointerOperand()];
    // We only really care about loads and stores
    return {};
  };


  auto computeGEN = [&](Instruction *s, noelle::DataFlowResult *df) {
    for (auto *ptr : get_used(s)) {
      df->GEN(s).insert(ptr);
    }
  };

  auto computeKILL = [&](Instruction *s, noelle::DataFlowResult *df) {
    // KILL(s): The set of variables that are assigned a value in s (in many books, KILL (s) is also
    // defined as the set of variables assigned a value in s before any use, but this does not
    // change the solution of the dataflow equation). In this, we don't care about KILL, as we are
    // operating on an SSA, where things are not redefined
    for (auto *src : sources) {
      if (s == src) df->KILL(s).insert(s);
    }
  };


  auto computeIN = [&](std::set<Value *> &IN, Instruction *s, noelle::DataFlowResult *df) {
    auto &gen = df->GEN(s);
    auto &out = df->OUT(s);
    auto &kill = df->KILL(s);

    // everything in GEN...
    IN.insert(gen.begin(), gen.end());

    // ...and everything in OUT that isn't in KILL
    for (auto o : out) {
      // if o is not found in KILL...
      if (kill.find(o) == kill.end()) {
        // ...add to IN
        IN.insert(o);
      }
    }
  };

  auto computeOUT = [&](std::set<Value *> &OUT, Instruction *p, noelle::DataFlowResult *df) {
    auto &INp = df->IN(p);
    OUT.insert(INp.begin(), INp.end());
  };

  auto *df =
      noelle::DataFlowEngine().applyBackward(&F, computeGEN, computeKILL, computeIN, computeOUT);


  for (auto *src : sources) {
    // if (s.accessed.find(src) == s.accessed.end()) {
    //   continue;
    // }
    auto tr = std::make_unique<alaska::Translation>();
    value_to_translation[src] = tr.get();  // icky pointer leak
    translations.push_back(std::move(tr));
  }


  for (auto &[ptr, tr] : value_to_translation) {
    // If the ptr is an argument, translation it in the first instruction
    if (auto argument = dyn_cast<Argument>(ptr)) {
      tr->translation =
          dyn_cast<CallInst>(insertTranslationBefore(&argument->getParent()->front().front(), ptr));
      errs() << "translation: " << *tr->translation << "\n";
    }


    for (auto *i : insts) {
      // Grab the IN and OUT sets
      auto &IN = df->IN(i);
      auto &OUT = df->OUT(i);
      // Compute membership in those sets
      bool in = (IN.find(ptr) != IN.end());
      bool out = (OUT.find(ptr) != OUT.end());

      // 1. Find locations to translate the pointer at.
      //    These are locations where the value is in the OUT but not the IN
      if (!in && out) {
        ALASKA_SANITY(tr->translation == NULL, "Translation was not null");
        // Put the translation after this instruction!
        tr->translation = dyn_cast<CallInst>(insertTranslationBefore(i->getNextNode(), ptr));
        errs() << "tr: " << *tr->translation << "\n";
      }

      // 2. If the instruction has the value in it's in set, but not it's
      // out set, it is a release location.
      if (!out && in) {
        insertReleaseBefore(i->getNextNode(), ptr);
        tr->releases.insert(dyn_cast<CallInst>(i->getNextNode()));
      }


      if (auto *branch = dyn_cast<BranchInst>(i)) {
        for (auto succ : branch->successors()) {
          break;
          auto succInst = &succ->front();
          auto &succIN = df->IN(succInst);
          // if it's not in the IN of succ, translate on the edge
          if (succIN.find(ptr) == succIN.end()) {
            // HACK: split the edge and insert on the branch
            BasicBlock *from = i->getParent();
            BasicBlock *to = succ;
            auto trampoline = llvm::SplitEdge(from, to);

            tr->releases.insert(
                dyn_cast<CallInst>(insertReleaseBefore(trampoline->getTerminator(), ptr)));
          }
        }
      }
    }
  }


  delete df;
}


void alaska::insertConservativeTranslations(llvm::Function &F) {
  alaska::PointerFlowGraph G(F);

  // Naively insert translate/release around loads and stores (the sinks in the graph provided)
  auto nodes = G.get_nodes();
  // Loop over all the nodes...
  for (auto node : nodes) {
    // only operate on sinks...
    if (node->type != alaska::Sink) continue;
    auto inst = dyn_cast<Instruction>(node->value);

    auto dbg = inst->getDebugLoc();
    // Insert the translate/release.
    // We have to handle load and store seperately, as their operand ordering is different
    // (annoyingly...)
    if (auto *load = dyn_cast<LoadInst>(inst)) {
      auto ptr = load->getPointerOperand();
      auto t = insertTranslationBefore(inst, ptr);
      load->setOperand(0, t);
      alaska::insertReleaseBefore(inst->getNextNode(), ptr);
      continue;
    }

    if (auto *store = dyn_cast<StoreInst>(inst)) {
      auto ptr = store->getPointerOperand();
      auto t = insertTranslationBefore(inst, ptr);
      store->setOperand(1, t);
      insertReleaseBefore(inst->getNextNode(), ptr);
      continue;
    }
  }
}
