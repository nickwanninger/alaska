#include <LockForest.h>
#include <Graph.h>
#include <Utils.h>
#include <deque>
#include <unordered_set>
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include <noelle/core/DataFlow.hpp>


static void extract_nodes(alaska::LockForest::Node *node, std::unordered_set<alaska::LockForest::Node *> &nodes) {
  nodes.insert(node);

  for (auto &child : node->children) {
    extract_nodes(child.get(), nodes);
  }
}

int alaska::LockForest::Node::depth(void) {
  if (parent == NULL) return 0;
  return parent->depth() + 1;
}

alaska::LockForest::Node *alaska::LockForest::Node::compute_shared_lock(void) {
  if (share_lock_with) {
    auto *s = share_lock_with->compute_shared_lock();
    if (s) return s;
    return share_lock_with;
  }
  return nullptr;
}

llvm::Instruction *alaska::LockForest::Node::effective_instruction(void) {
  if (auto inst = dyn_cast<llvm::Instruction>(val)) {
    if (auto phi = dyn_cast<llvm::PHINode>(val)) {
      return phi->getParent()->getFirstNonPHI();
    }
    return inst;
  }

  if (auto in_arg = dyn_cast<llvm::Argument>(this->val)) {
    auto *func = in_arg->getParent();
    auto &in_bb = func->getEntryBlock();
    return in_bb.getFirstNonPHI();
  }

  return NULL;
}

alaska::LockForest::Node::Node(alaska::FlowNode *val, alaska::LockForest::Node *parent) {
  this->val = val->value;
  this->parent = parent;
  for (auto *out : val->get_out_nodes()) {
    children.push_back(std::make_unique<Node>(out, this));
  }
}



static llvm::Loop *get_outermost_loop_for_lock(
    llvm::Loop *loopToCheck, llvm::Instruction *pointer, llvm::Instruction *user) {
  // first, check the `loopToCheck` loop. If it fits the requirements we are looking for, early return and don't check
  // subloops. If it doesn't, recurse down to the subloops and see if they fit.
  //
  // 1. if the loop contains the user but not the pointer, this is the best loop.
  //    (we want to lock right before the loop header)
  if (loopToCheck->contains(user) && !loopToCheck->contains(pointer)) {
    return loopToCheck;
  }

  for (auto *subLoop : loopToCheck->getSubLoops()) {
    if (auto *loop = get_outermost_loop_for_lock(subLoop, pointer, user)) {
      return loop;
    }
  }
  return nullptr;
}


static llvm::Instruction *compute_lock_insertion_location(
    llvm::Value *pointerToLock, llvm::Instruction *lockUser, llvm::LoopInfo &loops) {
  // the instruction to consider as the "location of the pointer". This is done for things like arguments.
  llvm::Instruction *effectivePointerInstruction = NULL;
  if (auto pointerToLockInst = dyn_cast<llvm::Instruction>(pointerToLock)) {
    effectivePointerInstruction = pointerToLockInst;
  } else {
    // get the first instruction in the function the argument is a part of;
    effectivePointerInstruction = lockUser->getParent()->getParent()->front().getFirstNonPHI();
  }
  ALASKA_SANITY(effectivePointerInstruction != NULL, "No effective instruction for pointerToLock");

  llvm::Loop *targetLoop = NULL;
  for (auto loop : loops) {
    targetLoop = get_outermost_loop_for_lock(loop, effectivePointerInstruction, lockUser);
    if (targetLoop != NULL) break;
  }

  // If no loop was found, lock at the lockUser.
  if (targetLoop == NULL) return lockUser;

  llvm::BasicBlock *incoming, *back;
  targetLoop->getIncomingAndBackEdge(incoming, back);


  // errs() << "Found loop for " << *pointerToLock << ": " << *targetLoop;
  if (incoming && incoming->getTerminator()) {
    return incoming->getTerminator();
  }

  return lockUser;
}



alaska::LockForest::LockForest(alaska::PointerFlowGraph &G, llvm::PostDominatorTree &PDT) : func(G.func()) {
  // Compute the dominator tree.
  // TODO: take this as an argument instead.
  llvm::DominatorTree DT(func);
  llvm::LoopInfo loops(DT);

  // The nodes which have no in edges that it post dominates
  std::unordered_set<alaska::FlowNode *> roots;

  // all the sources are roots in the flow tree
  for (auto *node : G.get_nodes()) {
    if (node->type == alaska::NodeType::Source) roots.insert(node);
  }

  // TODO: reduce redundant roots with alias analysis
  // TODO: have good alias analysis
  // TODO: be better

  // Create the forest from the roots, and compute dominance relationships among top-level siblings
  for (auto *root : roots) {
    auto node = std::make_unique<Node>(root);

    // compute which children dominate which siblings
    std::unordered_set<Node *> nodes;
    extract_nodes(node.get(), nodes);
    for (auto *node : nodes) {
      for (auto &child : node->children) {
        auto *inst = child->effective_instruction();
        for (auto &sibling : node->children) {
          if (sibling == child) continue;
          auto *sib_inst = sibling->effective_instruction();
          if (PDT.dominates(inst, sib_inst)) {
            child->postdominates.insert(sibling.get());
          }

          if (DT.dominates(inst, sib_inst)) {
            sibling->share_lock_with = child.get();
            child->dominates.insert(sibling.get());
          }
        }
      }
    }

    this->roots.push_back(std::move(node));
  }

  for (auto &root : this->roots) {
    for (auto &child : root->children) {
      if (child->share_lock_with == NULL) {
        auto &lb = get_lockbounds();
        lb.pointer = root->val;
        child->lock_id = lb.id;
      }
    }
    // Apply lockbounds to the forest nodes.
    for (auto &child : root->children) {
      unsigned id = child->lock_id;
      if (auto *share = child->compute_shared_lock()) {
        id = share->lock_id;
      }
      ALASKA_SANITY(id != UINT_MAX, "invalid lock id");
      child->set_lockid(id);
    }
  }


  // Compute the instruction at which a lock should be inserted to minimize the
  // number of runtime invocations, but to also minimize the number of handles
  // which are locked when alaska_barrier functions are called. There are a few
  // decisions it this function makes:
  //   - if @lockUser is inside a loop that @pointerToLock is not:
  //     - If the loop contains a call to `alaska_barrier`, lock before @lockUser
  //     - else, lock before the branch into the loop's header.
  //   - else, lock at @lockUser
  for (auto &root : this->roots) {
    for (auto &child : root->children) {
      auto *inst = dyn_cast<llvm::Instruction>(child->val);
      ALASKA_SANITY(inst != NULL, "child node has no instruction");
      // first, if the child does not share a lock with anyone, and
      // it's parent is a source, create the incoming lock
      if (child->share_lock_with == NULL && child->parent->parent == NULL) {
        auto &lb = get_lockbounds(child->lock_id);
        lb.lockBefore = compute_lock_insertion_location(root->val, inst, loops);
      }
    }
  }



  ///////////////////////////////////////////////////////////////////////////
  //                      Compute unlock locations                         //
  ///////////////////////////////////////////////////////////////////////////

  // used to compute GEN: (which node does an instruction use?)
  std::unordered_map<Instruction *, LockBounds *> inst2bounds;
  {
    std::unordered_set<Node *> nodes;
    for (auto &root : this->roots) {
      for (auto &child : root->children) {
        extract_nodes(child.get(), nodes);
      }
    }

    for (auto &node : nodes) {
      if (auto *inst = dyn_cast<Instruction>(node->val)) {
        inst2bounds[inst] = &get_lockbounds(node->lock_id);
      }
    }
    for (auto &[id, bounds] : locks) {
      inst2bounds[bounds->lockBefore] = bounds.get();
    }
  }


  // given an instruction, return the instruction that represents the lockbound value that it uses
  auto get_used = [&](Instruction *user) -> llvm::Value * {
    auto f = inst2bounds.find(user);
    if (f == inst2bounds.end()) {
      // errs() << "user has no bound: " << *user << "\n";
      return nullptr;
    }
    return f->second->lockBefore;
  };

  auto computeGEN = [&](Instruction *s, noelle::DataFlowResult *df) {
    if (auto *used = get_used(s)) {
      df->GEN(s).insert(used);
    }
  };

  auto computeKILL = [&](Instruction *s, noelle::DataFlowResult *df) {
    // KILL(s): The set of variables that are assigned a value in s (in many books, KILL (s) is also defined as the set
    // of variables assigned a value in s before any use, but this does not change the solution of the dataflow
    // equation). In this, we don't care about KILL, as we are operating on an SSA, where things are not redefined
    if (auto *used = get_used(s)) {
      if (used == s) {
        df->KILL(s).insert(used);
      }
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

  auto *df = noelle::DataFlowEngine().applyBackward(&func, computeGEN, computeKILL, computeIN, computeOUT);




  // auto get_lid = [&](llvm::Value *inst) {
  //   for (auto &[id, bounds] : locks) {
  //     if (bounds->lockBefore == inst) return id;
  //   }
  //   return UINT_MAX;
  // };
  //
  // auto dump_set = [&](const char *name, std::set<Value *> &s) {
  //   if (s.empty()) return;
  //   errs() << "    " << name << ":";
  //   for (auto v : s) {
  //     errs() << " " << get_lid(v);
  //     // errs() << " [" << *v << "]";
  //   }
  //   errs() << "\n";
  // };
  //
  // for (auto &BB : func) {
  //   for (auto &I : BB) {
  //     errs() << I << "\n";
  //     dump_set("GEN", df->GEN(&I));
  //     dump_set("KILL", df->KILL(&I));
  //     dump_set("IN", df->IN(&I));
  //     dump_set("OUT", df->OUT(&I));
  //   }
  // }



#if 0
  // using the dataflow result, insert unlock locations into the corresponding lockbounds
  // The way this works is we go over each lockbound, and find the instructions which
  // contain it's lockBefore instruction in the IN set, but not in their OUT set. These
  // instructions are the final user of the particular bound. The successor instruction
  // is the instruction that the unlock call is inserted before.
  for (auto &[id, lockbounds] : locks) {
    for (auto &BB : func) {
      for (auto &I : BB) {
        auto &IN = df->IN(&I);
        auto &OUT = df->OUT(&I);

        // if the value is not in the IN, skip
        if (IN.find(lockbounds->lockBefore) == IN.end()) {
          continue;
        }

        // if the value is not in the OUT, lock after this instruction
        if (OUT.find(lockbounds->lockBefore) == OUT.end()) {
          lockbounds->unlocks.insert(I.getNextNode());
          continue;
        }

        // special case for branches: insert an unlock on the "exit edge" of a branch
				// if the branch has the value in the OUT, but the branched-to instruction does
				// not have it in the IN.
        if (auto *branch = dyn_cast<BranchInst>(&I)) {
          for (auto succ : branch->successors()) {
            auto succInst = &succ->front();
            auto &succIN = df->IN(succInst);
            // if it's not in the IN of succ, lock on the edge
            if (succIN.find(lockbounds->lockBefore) == succIN.end()) {
              lockbounds->unlocks.insert(&I);
            }
          }
        }
      }
    }
  }
#endif

  // errs() << func << "\n";
  // for (auto &[id, lb] : locks) {
  //   errs() << "lid" << id << "\n";
  //   if (lb->lockBefore) errs() << " - lock:   " << *lb->lockBefore << "\n";
  //
  //   for (auto *before : lb->unlocks) {
  //     errs() << " - unlock: " << *before << "\n";
  //   }
  // }

  delete df;

#ifdef ALASKA_DUMP_FLOW_FOREST
  dump_dot();
#endif
}


alaska::LockBounds &alaska::LockForest::get_lockbounds(unsigned id) {
  auto f = locks.find(id);
  ALASKA_SANITY(f != locks.end(), "lock not found\n");
  return *f->second;
}

alaska::LockBounds &alaska::LockForest::get_lockbounds(void) {
  unsigned id = next_lock_id++;
  auto b = std::make_unique<LockBounds>();
  b->id = id;
  locks[id] = std::move(b);
  return get_lockbounds(id);
}

void alaska::LockForest::dump_dot(void) {
  alaska::println("------------------- { Flow forest for function ", func.getName(), " } -------------------");
  alaska::println("digraph {");
  alaska::println("  label=\"flow forest for ", func.getName(), "\";");
  alaska::println("  compound=true;");
  alaska::println("  start=1;");

  std::unordered_set<Node *> nodes;
  for (auto &root : roots) {
    extract_nodes(root.get(), nodes);
  }


  for (auto node : nodes) {
    errs() << "  n" << node << " [label=\"";
    errs() << "{" << *node->val;

    if (node->parent != NULL) {
      errs() << "|{lid" << node->lock_id << "}";
    }
    if (node->translated) {
      errs() << "|{tx:";
      errs() << *node->translated;
      errs() << "}";
    }
    if (node->parent == NULL && node->children.size() > 0) {
      errs() << "|{";
      int i = 0;

      for (auto &child : node->children) {
        if (i++ != 0) {
          errs() << "|";
        }

        errs() << "<n" << child.get() << ">";
        if (child->share_lock_with == NULL) {
          if (child->incoming_lock) {
            errs() << *child->incoming_lock;
          } else {
            errs() << "lid" << child->lock_id;
          }
        } else {
          errs() << "-";
        }
      }
      errs() << "}";
    }

    errs() << "}";
    errs() << "\", shape=record";
    errs() << "];\n";
  }

  for (auto node : nodes) {
    if (node->parent) {
      if (node->parent->parent == NULL && node->share_lock_with) {
        errs() << "  n" << node->parent << ":n" << node->compute_shared_lock() << " -> n" << node
               << " [style=\"dashed\", color=orange]\n";
      } else {
        errs() << "  n" << node->parent << ":n" << node << " -> n" << node << "\n";
      }
    }


    for (auto *dom : node->dominates) {
      errs() << "  n" << node << " -> n" << dom << " [color=red, label=\"D\", style=\"dashed\"]\n";
    }

    for (auto *dom : node->postdominates) {
      errs() << "  n" << node << " -> n" << dom << " [color=blue, label=\"PD\"]\n";
    }
  }


  alaska::println("}\n\n\n");
}
