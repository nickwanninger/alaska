#include <alaska/Translations.h>
#include <alaska/TranslationForest.h>
#include <alaska/PointerFlowGraph.h>
#include <alaska/Utils.h>

#include "llvm/IR/CFG.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include <noelle/core/DataFlow.hpp>
#include <noelle/core/DGBase.hpp>
#include <noelle/core/PDGPrinter.hpp>
#include "llvm/ADT/SparseBitVector.h"

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
  std::unordered_map<int, alaska::Translation*> id_map;



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
    BasicBlock* bb;
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


struct TranslationPDGConstructionVisitor
    : public llvm::InstVisitor<TranslationPDGConstructionVisitor> {
  PDG &pdg;
  TranslationPDGConstructionVisitor(PDG &pdg)
      : pdg(pdg) {
  }

  void look_at(llvm::Value *val) {
    // add the value to the pdg right away. This will ensure that circular dependencies don't
    // infinitely recurse -- hopefully...!
    pdg.addNode(val, 1);

    if (auto *inst = dyn_cast<Instruction>(val)) {
      this->visit(inst);
    }
  }

  void add_use(llvm::Use &use) {
    auto *user = dyn_cast<Value>(use.getUser());  // grab the dependant
    auto *value = use.get();                      // Grab the depnedency
    if (pdg.fetchNode(value) == NULL) {
      // we need to visit the node!
      look_at(value);
    }

    pdg.addEdge(user, value);
  }

  void visitLoadInst(llvm::LoadInst &I) {
    add_use(I.getOperandUse(0));
  }

  void visitStoreInst(llvm::StoreInst &I) {
    add_use(I.getOperandUse(1));
  }

  void visitGetElementPtrInst(llvm::GetElementPtrInst &I) {
    auto &use = I.getOperandUse(0);
    add_use(use);
  }

  void visitPHINode(llvm::PHINode &I) {
    for (unsigned i = 0; i < I.getNumIncomingValues(); i++) {
      auto &use = I.getOperandUse(i);
      add_use(use);
    }
  }

  // all else
  void visitInstruction(llvm::Instruction &I) {
    // Do nothing. Unhandled instructions have no incoming edges and
    // are therefore roots of the tree that must be translated.
  }
};


void alaska::insertHoistedTranslations(llvm::Function &F) {
  alaska::TranslationForest forest(F);
  (void)forest.apply();
  return;
  // if (F.getName() != "sglib_ilist_sort") return;
  // if (F.getName() != "foo") return;

  // alaska::PointerFlowGraph G(F);
  // llvm::DominatorTree DT(F);
  // llvm::PostDominatorTree PDT(F);
  // llvm::LoopInfo loops(DT);

  PDG pdg;

  TranslationPDGConstructionVisitor vis(pdg);
  std::vector<llvm::Instruction *> memoryInsts;

  for (auto &BB : F) {
    for (auto &I : BB) {
      if (auto load = dyn_cast<LoadInst>(&I)) {
        vis.look_at(load);
        memoryInsts.push_back(load);
      }

      if (auto store = dyn_cast<StoreInst>(&I)) {
        vis.look_at(store);
        memoryInsts.push_back(store);
      }
    }
  }

  std::string name;
  name += ".pdg.";
  name += F.getName().data();
  name += ".dot";
  errs() << "output PDG to " << name << "\n";

  DGPrinter::writeGraph<PDG, llvm::Value>(name, &pdg);

  // Now that we have the PDG, insert roots.
  //
  // tmap: the mapping from a value in the PDG (handle space) to either `alaska.*` calls or a
  // phi/select statement that operates in pointer space.
  std::unordered_map<llvm::Value *, llvm::Instruction *> tmap;
  std::vector<noelle::DGNode<Value> *> phiNodes;


  auto getSoleDependency = [](noelle::DGNode<Value> *node) {
    ALASKA_SANITY(node->numOutgoingEdges() == 1, "Node does not have a depnedency");
    return (*node->begin_outgoing_edges())->getIncomingNode();
  };


  // doTranslation: given an instruction, return it's value in tmap. If there is not
  // a value in tmap yet, create it and return it. This could result in recursive calls
  // to this function but it will reach a fixed point as we patch-up PHI/Select statements
  // later. Those instructions must be handled specially later because they can create
  // loops in dependencies, and could result in an infinite recursion here.
  std::function<Instruction *(noelle::DGNode<Value> *)> doTranslation =
      [&](noelle::DGNode<Value> *node) -> llvm::Instruction * {
    auto *nodeV = node->getT();
    if (tmap.contains(nodeV)) {
      // TODO: double lookup!
      return tmap[nodeV];
    }


    if (auto *phi = dyn_cast<PHINode>(nodeV)) {
      // First, we insert a PHINode after this one, but give it no arguments. This is temporarially
      // invalid, but we will patch it later.
      IRBuilder<> b(phi);
      auto newPhi = b.CreatePHI(phi->getType(), phi->getNumIncomingValues());
      tmap[phi] = newPhi;

      // Ensure the dependencies of this PHI are translated and create the edges. This is safe to do
      // as we added the tmap entry for newPhi so recursive references will work.
      for (unsigned i = 0; i < phi->getNumIncomingValues(); i++) {
        auto ptr = phi->getIncomingValue(i);
        if (auto node = pdg.fetchNode(ptr)) {
          ptr = doTranslation(node);
        }
        auto blk = phi->getIncomingBlock(i);

        newPhi->addIncoming(ptr, blk);
      }


      return newPhi;
    } else if (auto *gep = dyn_cast<GetElementPtrInst>(nodeV)) {
      ALASKA_SANITY(node->numOutgoingEdges() == 1, "A GEP must have only one incoming value");
      auto incoming = getSoleDependency(node);
      auto ptr = doTranslation(incoming);

      auto derive = alaska::insertDerivedBefore(gep->getNextNode(), ptr, gep);
      tmap[gep] = derive;
      return derive;

    } else {
      llvm::Instruction *insertBefore = NULL;

      if (auto leafI = dyn_cast<llvm::Instruction>(nodeV)) {
        insertBefore = leafI->getNextNode();
      } else {
        // It was not an instruction. We need to insert in the first non-phi in the function as it
        // must be an argument and they are not instructions
        insertBefore = F.front().getFirstNonPHIOrDbgOrLifetime();
      }

      // First, insert an `alaska.root` call. This forces the translation itself to be
      // based on an instruction and is basically just a normalization feature.
      // It also helps disambiguate roots that might be used in different
      // translation calls from eachother.
      auto *handle = alaska::insertRootBefore(insertBefore, nodeV);
      // Then, insert a call to translate.
      auto *translate = alaska::insertTranslationBefore(insertBefore, handle);
      // Record the pointer-space value to be used in the tmap.
      tmap[nodeV] = translate;
      // alaska::println("Unknown", *nodeV);
      return translate;
    }


    return NULL;
  };



  for (auto *inst : memoryInsts) {
    auto *node = pdg.fetchNode(inst);
    // ALASKA_SANITY(node->numOutgoingEdges() == 1, "A sink node must have only one outoing edge");

    auto dep = getSoleDependency(node);
    // alaska::println("op: ", *inst);
    // alaska::println("dep: ", *dep->getT());
    auto ptr = doTranslation(dep);
    if (auto loadInst = dyn_cast<LoadInst>(inst)) {
      // alaska::println("load ", *loadInst);
      loadInst->setOperand(0, ptr);
    } else if (auto storeInst = dyn_cast<StoreInst>(inst)) {
      // alaska::println("store ", *storeInst);
      storeInst->setOperand(1, ptr);
    } else {
      alaska::println("unhandled: ", *inst);
      abort();
    }
  }
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
