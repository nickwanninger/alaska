#include <Locks.h>
#include <LockForest.h>
#include <Utils.h>
#include "Graph.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include <noelle/core/DataFlow.hpp>

llvm::Value *alaska::Lock::getHandle(void) {
  // the pointer for this lock is simply the first argument of the lock call
  return lock->getOperand(0);
}

llvm::Value *alaska::Lock::getPointer(void) {
  // the handle is the return value of the lock call
  return lock;
}


llvm::Function *alaska::Lock::getFunction(void) {
  if (lock == NULL) return NULL;
  return lock->getFunction();
}



// static void apply_lock_r(alaska::Lock &lock, llvm::Value *incoming, llvm::Instruction *inst);

struct LockApplicationVisitor : public llvm::InstVisitor<LockApplicationVisitor> {
  alaska::Lock &lock;
  llvm::Value *incoming = NULL;

  LockApplicationVisitor(alaska::Lock &lock, llvm::Value *incoming) : lock(lock), incoming(incoming) {}

  void visitGetElementPtrInst(llvm::GetElementPtrInst &I) {
    // create a new GEP right after this one.
    IRBuilder<> b(I.getNextNode());

    std::vector<llvm::Value *> inds(I.idx_begin(), I.idx_end());
    auto new_incoming =
        dyn_cast<Instruction>(b.CreateGEP(I.getSourceElementType(), incoming, inds, "", I.isInBounds()));

    LockApplicationVisitor vis(lock, new_incoming);
    for (auto user : I.users()) {
      vis.visit(dyn_cast<Instruction>(user));
    }
  }

  void visitLoadInst(llvm::LoadInst &I) {
    lock.users.insert(&I);
    I.setOperand(0, incoming);
  }
  void visitStoreInst(llvm::StoreInst &I) {
    lock.users.insert(&I);
    I.setOperand(1, incoming);
  }

  void visitInstruction(llvm::Instruction &I) { alaska::println("dunno how to handle this: ", I); }
};


// static void apply_lock_r(alaska::Lock &lock, llvm::Value *incoming, llvm::Instruction *inst) {
//   if (lock.users.find(inst) != lock.users.end()) {
//     return;
//   }
//   lock.users.insert(inst);
//   LockApplicationVisitor vis(lock, incoming);
//   for (auto user : inst->users()) {
//     vis.visit(dyn_cast<Instruction>(user));
//   }
// }

void alaska::Lock::apply() {
  // don't apply the lock if it was already applied (has users)
  if (users.size() != 0) {
    return;
  }
}



void alaska::Lock::computeLiveness(void) {
  if (auto *func = getFunction()) {
    std::vector<alaska::Lock *> lps = {this};
    alaska::computeLockLiveness(*func, lps);
  }
}


void alaska::Lock::remove(void) {
  auto *handle = getHandle();
  auto *pointer = getPointer();

  pointer->replaceAllUsesWith(handle);



  // erase lock and unlock calls from the parent
  for (auto u : unlocks) {
    u->eraseFromParent();
  }
  lock->eraseFromParent();


  this->lock = nullptr;
  this->unlocks.clear();
  this->users.clear();
  this->liveInstructions.clear();
}


bool alaska::Lock::isUser(llvm::Instruction *inst) { return users.find(inst) != users.end(); }

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

std::vector<std::unique_ptr<alaska::Lock>> alaska::extractLocks(llvm::Function &F) {
  auto &M = *F.getParent();

  // find the lock function
  auto *lockFunction = M.getFunction("alaska_lock");
  auto *unlockFunction = M.getFunction("alaska_unlock");
  if (lockFunction == NULL) {
    // if it doesn't exist, there aren't any locks so early return
    return {};
  }

  std::vector<std::unique_ptr<alaska::Lock>> locks;

  // Find all users of lock that are in this function
  for (auto user : lockFunction->users()) {
    if (auto inst = dyn_cast<CallInst>(user)) {
      if (inst->getFunction() == &F) {
        if (inst->getCalledFunction() == lockFunction) {
          auto l = std::make_unique<alaska::Lock>();
          l->lock = inst;
          locks.push_back(std::move(l));
        }
      }
    }
  }

  // associate unlocks
  if (unlockFunction != NULL) {
    for (auto user : unlockFunction->users()) {
      if (auto inst = dyn_cast<CallInst>(user)) {
        if (inst->getFunction() == &F) {
          if (inst->getCalledFunction() == unlockFunction) {
            for (auto &lk : locks) {
              if (lk->getHandle() == inst->getOperand(0)) {
                lk->unlocks.insert(inst);
              }
            }
          }
        }
      }
    }
  }

  for (auto &lk : locks) {
    extract_users(lk->lock, lk->users);
  }

  computeLockLiveness(F, locks);

  return locks;
}


void alaska::computeLockLiveness(llvm::Function &F, std::vector<std::unique_ptr<alaska::Lock>> &locks) {
  std::vector<alaska::Lock *> lps;

  for (auto &l : locks)
    lps.push_back(l.get());
  return computeLockLiveness(F, lps);
}

void alaska::computeLockLiveness(llvm::Function &F, std::vector<alaska::Lock *> &locks) {
  // mapping from instructions to the lock they use.
  std::map<llvm::Instruction *, std::set<alaska::Lock *>> inst_to_lock;
  // map from the call to alaska_lock to the lock structure it belongs to.
  std::map<llvm::Instruction *, alaska::Lock *> lock_inst_to_lock;
  for (auto &lock : locks) {
    // clear the existing liveness info
    lock->liveInstructions.clear();
    lock_inst_to_lock[lock->lock] = lock;

    for (auto &user : lock->users) {
      inst_to_lock[user].insert(lock);
    }
  }


  // given an instruction, return the instruction that represents the lockbound value that it uses
  auto get_used = [&](Instruction *user) -> std::set<alaska::Lock *> {
    if (inst_to_lock.find(user) != inst_to_lock.end()) {
      return inst_to_lock[user];  // SLOW: double lookups
    }
    return {};
  };

  auto computeGEN = [&](Instruction *s, noelle::DataFlowResult *df) {
    for (auto *lock : get_used(s)) {
      df->GEN(s).insert(lock->lock);
    }
  };

  auto computeKILL = [&](Instruction *s, noelle::DataFlowResult *df) {
    // KILL(s): The set of variables that are assigned a value in s (in many books, KILL (s) is also defined as the set
    // of variables assigned a value in s before any use, but this does not change the solution of the dataflow
    // equation). In this, we don't care about KILL, as we are operating on an SSA, where things are not redefined
    if (lock_inst_to_lock.find(s) != lock_inst_to_lock.end()) {
      df->KILL(s).insert(s);
    }
    // for (auto *lock : get_used(s)) {
    //   auto used = lock->lock;
    //   if (used == s) {
    //     df->KILL(s).insert(used);
    //   }
    // }
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

  auto *df = noelle::DataFlowEngine().applyBackward(&F, computeGEN, computeKILL, computeIN, computeOUT);

  for (auto &BB : F) {
    for (auto &I : BB) {
      for (auto l : df->IN(&I)) {
        auto inst = dyn_cast<Instruction>(l);
        lock_inst_to_lock[inst]->liveInstructions.insert(&I);
      }
    }
  }

  delete df;
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

void alaska::printLockDot(llvm::Function &F, std::vector<std::unique_ptr<alaska::Lock>> &locks) {
  std::map<alaska::Lock *, std::string> colors;

  for (size_t i = 0; i < locks.size(); i++) {
    colors[locks[i].get()] = random_background_color();
  }


  alaska::println("digraph {");
  alaska::println("  label=\"Locks in ", F.getName(), "\";");
  alaska::println("  compound=true;");
  alaska::println("  graph [fontsize=8 fontname=\"Courier\"];");  // splines=\"ortho\"];");
  alaska::println("  node[fontsize=9,shape=none,style=filled,fillcolor=white];");


  for (auto &BB : F) {
    alaska::println("  n", &BB, " [label=<");
    alaska::println("    <table fontname=\"Courier\" border=\"1\" cellspacing=\"0\" padding=\"0\">");

    // generate the label
    //
    alaska::print("      <tr><td align=\"left\" port=\"header\" border=\"0\">");
    BB.printAsOperand(errs(), false);
    alaska::print(":</td>");
    for (auto &v : colors) {
      (void)v;
      alaska::print("<td align=\"left\" border=\"0\"></td>");
    }
    alaska::println("</tr>");

    for (auto &I : BB) {
      std::string color = "white";

      for (auto &[lock, lcolor] : colors) {
        if (lock->lock == &I) {
          color = lcolor;
          break;
        }

        // if (lock->isUser(&I)) {
        //   color = lcolor;
        //   break;
        // }

        for (auto &unlock : lock->unlocks) {
          if (unlock == &I) {
            color = lcolor;
          }
        }
      }
      alaska::print("      <tr><td align=\"left\" port=\"n", &I, "\" border=\"0\" bgcolor=\"", color, "\">  ",
          escape(I), "</td>");


      for (auto &[lock, color] : colors) {
        std::string c = "white";

        if (lock->liveInstructions.find(&I) != lock->liveInstructions.end()) {
          c = color;
        }
        alaska::print("<td align=\"left\" border=\"0\" bgcolor=\"", c, "\">  </td>");
      }

      alaska::println("</tr>");
    }
    alaska::println("    </table>>];");  // end label
  }


  for (auto &BB : F) {
    auto *term = BB.getTerminator();
    for (unsigned i = 0; i < term->getNumSuccessors(); i++) {
      auto *other = term->getSuccessor(i);
      alaska::println("  n", &BB, ":n", term, " -> n", other, ":header");
    }
  }
  alaska::println("}");
}

void alaska::insertHoistedLocks(llvm::Function &F) {
  alaska::LockForest forest(F);
  auto locks = forest.apply();

  // for (auto &l : locks) {
  //   l->apply();
  // }
}


void alaska::insertConservativeLocks(llvm::Function &F) {
  alaska::PointerFlowGraph G(F);

  // Naively insert get/unlock around loads and stores (the sinks in the graph provided)
  auto nodes = G.get_nodes();
  // Loop over all the nodes...
  for (auto node : nodes) {
    // only operate on sinks...
    if (node->type != alaska::Sink) continue;
    auto inst = dyn_cast<Instruction>(node->value);

    auto dbg = inst->getDebugLoc();
    // Insert the get/unlock.
    // We have to handle load and store seperately, as their operand ordering is different (annoyingly...)
    if (auto *load = dyn_cast<LoadInst>(inst)) {
      auto ptr = load->getPointerOperand();
      auto t = insertLockBefore(inst, ptr);
      load->setOperand(0, t);
      alaska::insertUnlockBefore(inst->getNextNode(), ptr);
      continue;
    }

    if (auto *store = dyn_cast<StoreInst>(inst)) {
      auto ptr = store->getPointerOperand();
      auto t = insertLockBefore(inst, ptr);
      store->setOperand(1, t);
      insertUnlockBefore(inst->getNextNode(), ptr);
      continue;
    }
  }
}
