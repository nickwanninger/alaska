#include <Locks.h>
#include <LockForest.h>
#include <Utils.h>
#include "Graph.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
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
  // for (auto u : unlocks) {
  //   u->eraseFromParent();
  // }
  // lock->eraseFromParent();


  this->lock = nullptr;
  this->unlocks.clear();
  this->users.clear();
  this->liveInstructions.clear();
}


bool alaska::Lock::isUser(llvm::Instruction *inst) { return users.find(inst) != users.end(); }
bool alaska::Lock::isLive(llvm::Instruction *inst) { return liveInstructions.find(inst) != liveInstructions.end(); }

llvm::Value *alaska::Lock::getRootAllocation() {
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

std::vector<std::unique_ptr<alaska::Lock>> alaska::extractLocks(llvm::Function &F) {
  auto &M = *F.getParent();

  // find the lock function
  auto *getFunction = M.getFunction("alaska_translate");
  auto *putFunction = M.getFunction("alaska_release");
  if (getFunction == NULL) {
    // if it doesn't exist, there aren't any locks so early return
    return {};
  }

  std::vector<std::unique_ptr<alaska::Lock>> locks;

  // Find all users of lock that are in this function
  for (auto user : getFunction->users()) {
    if (auto inst = dyn_cast<CallInst>(user)) {
      if (inst->getFunction() == &F) {
        if (inst->getCalledFunction() == getFunction) {
          auto l = std::make_unique<alaska::Lock>();
          l->lock = inst;
          locks.push_back(std::move(l));
        }
      }
    }
  }

  // associate unlocks
  if (putFunction != NULL) {
    for (auto user : putFunction->users()) {
      if (auto inst = dyn_cast<CallInst>(user)) {
        if (inst->getFunction() == &F) {
          if (inst->getCalledFunction() == putFunction) {
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
  // map from the call to alaska_translate to the lock structure it belongs to.
  std::map<llvm::Instruction *, alaska::Lock *> lock_inst_to_lock;

  for (auto &lock : locks) {
    // clear the existing liveness info
    lock->liveInstructions.clear();
    lock_inst_to_lock[lock->lock] = lock;

    for (auto &user : lock->users) {
      inst_to_lock[user].insert(lock);
    }
  }


  auto get_lock_of = [](llvm::Instruction *inst) -> llvm::Value * {
    if (auto call = dyn_cast<CallInst>(inst)) {
      auto func = call->getCalledFunction();
      if (func && func->getName() == "alaska_translate") {
        return call->getArgOperandUse(0);
      }
    }

    return nullptr;
  };

  auto get_unlock_of = [](llvm::Instruction *inst) -> llvm::Value * {
    if (auto call = dyn_cast<CallInst>(inst)) {
      auto func = call->getCalledFunction();
      if (func && func->getName() == "alaska_release") {
        return call->getArgOperandUse(0);
      }
    }

    return nullptr;
  };


  // TODO: gen is every unlock
  auto computeGEN = [&](Instruction *s, noelle::DataFlowResult *df) {
    if (auto v = get_unlock_of(s)) {
      df->GEN(s).insert(v);
    }
  };

  // TODO: Kill is every lock
  auto computeKILL = [&](Instruction *s, noelle::DataFlowResult *df) {
    if (auto v = get_lock_of(s)) {
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

  auto *df = noelle::DataFlowEngine().applyBackward(&F, computeGEN, computeKILL, computeIN, computeOUT);

  // I don't like this.
  for (auto &[inst, values] : df->IN()) {
    for (auto l : values) {
      for (auto &lock : locks) {
        if (lock->getHandle() == l) {
          lock->liveInstructions.insert(inst);
        }
      }
    }
  }


  delete df;
}



bool alaska::shouldLock(llvm::Value *val) {
  if (!val->getType()->isPointerTy()) return false;
  if (dyn_cast<GlobalValue>(val)) return false;
  if (dyn_cast<AllocaInst>(val)) return false;
  if (dyn_cast<ConstantPointerNull>(val)) return false;

  if (auto gep = dyn_cast<GetElementPtrInst>(val)) {
    return shouldLock(gep->getPointerOperand());
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

  void alaska::printLockDot(
      llvm::Function &F, std::vector<std::unique_ptr<alaska::Lock>> &locks, llvm::raw_ostream &out) {
    std::map<alaska::Lock *, std::string> colors;

    for (size_t i = 0; i < locks.size(); i++) {
      colors[locks[i].get()] = random_background_color();
    }


    alaska::fprintln(out, "digraph {");
    alaska::fprintln(out, "  label=\"Locks in ", F.getName(), "\";");
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

        for (auto &[lock, lcolor] : colors) {
          if (lock->lock == &I) {
            color = lcolor;
            break;
          }
          for (auto &unlock : lock->unlocks) {
            if (unlock == &I) {
              color = lcolor;
            }
          }
        }
        alaska::fprint(out, "      <tr><td align=\"left\" port=\"n", &I, "\" border=\"0\" bgcolor=\"", color, "\">  ",
            escape(I), "</td>");


        for (auto &[lock, color] : colors) {
          if (lock->liveInstructions.find(&I) != lock->liveInstructions.end() || &I == lock->lock ||
              lock->unlocks.find(dyn_cast<CallInst>(&I)) != lock->unlocks.end()) {
            alaska::fprint(out, "<td align=\"left\" border=\"0\" bgcolor=\"", color, "\"> ");
            lock->getHandle()->printAsOperand(out);
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
      // Nothing. Don't lock allocas
    }

    void visitCallInst(llvm::CallInst &I) {
      // TODO: skip lock calls
      auto lock = I.getFunction()->getParent()->getFunction("alaska_translate");
      if (lock != NULL && I.getCalledFunction() == lock) {
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


  void alaska::insertHoistedLocks(llvm::Function &F) {
    alaska::LockForest forest(F);
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

    std::vector<std::unique_ptr<alaska::Lock>> locks;
    std::map<llvm::Value *, alaska::Lock *> value2lock;

    std::map<llvm::Value *, std::set<llvm::Value *>> sourceAliases;

    auto &sources = s.srcs;
    // alaska::println("Potential sources for ", F.getName(), ":");
    for (auto &src : sources) {
      // If the source was not accessed, ignore it.
      // if (s.accessed.find(src) == s.accessed.end()) {
      //   continue;
      // }
      sourceAliases[src].insert(src);
      // alaska::println("  - ", simpleFormat(src));
      TransientMappingVisitor tv;
      tv.look_at(src);
      // alaska::println("    Transients:");
      for (auto &t : tv.transients) {
        sourceAliases[t].insert(src);
        // alaska::println("       - ", simpleFormat(t));
      }
    }


    // alaska::println("Source Aliases:");
    // for (auto &[a, srcs] : sourceAliases) {
    //   alaska::println(*a);
    //   for (auto &src : srcs) {
    //     alaska::println("    - ", simpleFormat(src));
    //   }
    // }


    auto get_used = [&](llvm::Instruction *s) -> std::set<llvm::Value *> {
      if (auto load = dyn_cast<LoadInst>(s)) return sourceAliases[load->getPointerOperand()];
      if (auto store = dyn_cast<StoreInst>(s)) return sourceAliases[store->getPointerOperand()];
      // TODO: handle when it is an argument? Maybe introduce recursive lock call elimination here?
      return {};
    };


    auto computeGEN = [&](Instruction *s, noelle::DataFlowResult *df) {
      for (auto *ptr : get_used(s)) {
        df->GEN(s).insert(ptr);
      }
    };

    auto computeKILL = [&](Instruction *s, noelle::DataFlowResult *df) {
      // KILL(s): The set of variables that are assigned a value in s (in many books, KILL (s) is also defined as the
      // set of variables assigned a value in s before any use, but this does not change the solution of the dataflow
      // equation). In this, we don't care about KILL, as we are operating on an SSA, where things are not redefined
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

    auto *df = noelle::DataFlowEngine().applyBackward(&F, computeGEN, computeKILL, computeIN, computeOUT);


    for (auto *src : sources) {
      // if (s.accessed.find(src) == s.accessed.end()) {
      //   continue;
      // }
      auto lock = std::make_unique<alaska::Lock>();
      value2lock[src] = lock.get();  // icky pointer leak
      locks.push_back(std::move(lock));
    }


    // for (auto &BB : F) {
    //   BB.printAsOperand(errs());
    //   errs() << "\n";
    //   for (auto &I : BB) {
    //     alaska::println(I);
    //     for (auto l : df->IN(&I)) {
    //       alaska::println("       IN ", *l);
    //     }
    //
    //     for (auto l : df->OUT(&I)) {
    //       alaska::println("      OUT ", *l);
    //     }
    //   }
    // }

    for (auto &[ptr, lock] : value2lock) {
      // If the lock is an argument, lock it in the first instruction
      //
      if (auto argument = dyn_cast<Argument>(ptr)) {
        lock->lock = dyn_cast<CallInst>(insertLockBefore(&argument->getParent()->front().front(), ptr));
        errs() << "lock: " << *lock->lock << "\n";
      }


      for (auto *i : insts) {
        // Grab the IN and OUT sets
        auto &IN = df->IN(i);
        auto &OUT = df->OUT(i);
        // Compute membership in those sets
        bool in = (IN.find(ptr) != IN.end());
        bool out = (OUT.find(ptr) != OUT.end());

        // 1. Find locations to lock the lock at.
        //    These are locations where the value is in the OUT but not the IN
        if (!in && out) {
          ALASKA_SANITY(lock->lock == NULL, "Lock was not null");
          // Put the lock after this instruction!
          lock->lock = dyn_cast<CallInst>(insertLockBefore(i->getNextNode(), ptr));
          errs() << "lock: " << *lock->lock << "\n";
        }

        // 2. If the instruction has the value in it's in set, but not it's out set, it is an unlock location.
        if (!out && in) {
          insertUnlockBefore(i->getNextNode(), ptr);
          lock->unlocks.insert(dyn_cast<CallInst>(i->getNextNode()));
        }


        if (auto *branch = dyn_cast<BranchInst>(i)) {
          for (auto succ : branch->successors()) {
            break;
            auto succInst = &succ->front();
            auto &succIN = df->IN(succInst);
            // if it's not in the IN of succ, lock on the edge
            if (succIN.find(ptr) == succIN.end()) {
              // HACK: split the edge and insert on the branch
              BasicBlock *from = i->getParent();
              BasicBlock *to = succ;
              auto trampoline = llvm::SplitEdge(from, to);

              // lockinsertUnlockBefore(i->getNextNode(), ptr);
              // lock->unlocks.insert(dyn_cast<CallInst>(i->getNextNode()));
              lock->unlocks.insert(dyn_cast<CallInst>(insertUnlockBefore(trampoline->getTerminator(), ptr)));
            }
          }
        }
      }
    }


    // for (auto &lock : locks) {
    //   lock->apply();
    // }

    delete df;

    // errs() << F << "\n";

    // return;
    //
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
