#include <Locks.h>
#include <LockForest.h>
#include <Utils.h>
#include "Graph.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Transforms/Utils/Cloning.h"

llvm::Value *alaska::Lock::getHandle(void) {
  // the handle is the return value of the lock call
  return lock;
}

llvm::Value *alaska::Lock::getPointer(void) {
  // the pointer for this lock is simply the first argument of the lock call
  return lock->getOperand(0);
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
  for (auto user : unlockFunction->users()) {
    if (auto inst = dyn_cast<CallInst>(user)) {
      if (inst->getFunction() == &F) {
        if (inst->getCalledFunction() == unlockFunction) {
          for (auto &lk : locks) {
            if (lk->getPointer() == inst->getOperand(0)) {
              alaska::println("associate unlock:");
              alaska::println(*lk->lock);
              alaska::println(*inst);
              alaska::println();

              lk->unlocks.insert(inst);
            }
          }
        }
      }
    }
  }

  for (auto &lk : locks) {
    extract_users(lk->lock, lk->users);
  }


  return locks;
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

void alaska::printLockDot(llvm::Function &F, std::vector<std::unique_ptr<alaska::Lock>> &locks) {
  std::map<alaska::Lock *, const char *> colors;

  for (size_t i = 0; i < locks.size(); i++) {
    if (i > graphviz_colors.size()) {
      colors[locks[i].get()] = "gray";
    } else {
      colors[locks[i].get()] = graphviz_colors[i];
    }
  }


  alaska::println("digraph {");
  alaska::println("  label=\"Locks in ", F.getName(), "\";");
  alaska::println("  compound=true;");
  alaska::println("  graph[nodesep=1];");
  alaska::println("  node[fontsize=9,shape=none,style=filled,fillcolor=white];");


  for (auto &BB : F) {
    alaska::println("  n", &BB, " [label=<");
    alaska::println("    <table border=\"1\" cellspacing=\"0\" padding=\"0\">");

    // generate the label
    //
    alaska::print("      <tr><td align=\"left\" port=\"header\" border=\"0\">");
    BB.printAsOperand(errs(), false);
    alaska::println(":</td></tr>");

    for (auto &I : BB) {
      const char *color = "white";

      for (auto &[lock, lcolor] : colors) {
        if (lock->lock == &I) {
          color = lcolor;
        }

        if (lock->isUser(&I)) {
          color = lcolor;
        }

        for (auto &unlock : lock->unlocks) {
          if (unlock == &I) {
            color = lcolor;
          }
        }
      }
      alaska::println("      <tr><td align=\"left\" port=\"n", &I, "\" border=\"0\" bgcolor=\"", color, "\">  ",
          escape(I), "</td></tr>");
    }
    alaska::println("    </table>>];");  // end label
  }


  for (auto &BB : F) {
    auto *term = BB.getTerminator();
    for (unsigned i = 0; i < term->getNumSuccessors(); i++) {
      auto *other = term->getSuccessor(i);
      alaska::println("  n", &BB, ":n", term, " -> n", other);
    }
  }
  alaska::println("}");
}

void alaska::insertHoistedLocks(llvm::Function &F) {
  alaska::LockForest forest(F);
  forest.apply();
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