#include <LockForest.h>
#include <Graph.h>
#include <Utils.h>
#include <deque>
#include <unordered_set>
#include <sstream>
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
  // return lockUser;
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


#if 0
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
    // return;
    // IN[s] = GEN[s] U (OUT[s] - KILL[s])

    auto &gen = df->GEN(s);
    auto &out = df->OUT(s);
    auto &kill = df->KILL(s);
    IN.clear();

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

  auto computeOUT = [&](std::set<Value *> &OUT, Instruction *succ, noelle::DataFlowResult *df) {
    // return;
    // OUT[s] = IN[p] where p is every successor of s
    auto &INp = df->IN(succ);
    OUT.insert(INp.begin(), INp.end());
  };
#endif


  std::map<Instruction *, std::set<unsigned>> GEN;
  std::map<Instruction *, std::set<unsigned>> KILL;
  std::map<Instruction *, std::set<unsigned>> IN;
  std::map<Instruction *, std::set<unsigned>> OUT;

  std::set<Instruction *> todo;

  // here, we do something horrible. we insert lock instructions just so we can do analysis. Later we will delete them
  for (auto &[lid, lock] : locks) {
    lock->locked = insertLockBefore(lock->lockBefore, lock->pointer);
    KILL[lock->locked].insert(lid);
  }

  // initialize gen sets
  for (auto &BB : func) {
    for (auto &I : BB) {
      todo.insert(&I);
      auto f = inst2bounds.find(&I);
      if (f != inst2bounds.end()) {
        auto &lb = f->second;
        GEN[&I].insert(lb->id);
      }
    }
  }


  while (!todo.empty()) {
    // Grab some instruction from worklist...
    auto i = *todo.begin();
    // ...remove it from the worklist
    todo.erase(i);

    auto old_out = OUT[i];
    auto old_in = IN[i];
    // IN = GEN U (OUT - KILL);
    IN[i] = GEN[i];
    for (auto &v : OUT[i])
      if (KILL[i].find(v) == KILL[i].end()) IN[i].insert(v);

    OUT[i].clear();

    // successor updating
    // errs() << "succ of " << *i << "\n";
    if (auto *next_node = i->getNextNode()) {
      // errs() << "  - " << *next_node << "\n";
      for (auto &v : IN[next_node])
        OUT[i].insert(v);
    } else {
      // end of a basic block, get all successor basic blocks
      for (auto *bb : successors(i)) {
        // errs() << "  - " << bb->front() << "\n";
        for (auto &v : IN[&bb->front()])
          OUT[i].insert(v);
      }
    }

    if (old_out != OUT[i] || old_in != IN[i]) {
      todo.insert(i);
      // add all preds to the workqueue
      if (auto *prev_node = i->getPrevNode()) {
        todo.insert(prev_node);
      } else {
        // start of a basic block, get all predecessor basic blocks
        for (auto *bb : predecessors(i->getParent())) {
          todo.insert(bb->getTerminator());
        }
      }
    }
  }


  for (auto &[lid, lock] : locks) {
    lock->locked->eraseFromParent();
    lock->locked = NULL;
  }



#if 1
  // using the dataflow result, insert unlock locations into the corresponding lockbounds
  // The way this works is we go over each lockbound, and find the instructions which
  // contain it's lockBefore instruction in the IN set, but not in their OUT set. These
  // instructions are the final user of the particular bound. The successor instruction
  // is the instruction that the unlock call is inserted before.
  for (auto &[id, lockbounds] : locks) {
    for (auto &BB : func) {
      for (auto &I : BB) {
        auto &iIN = IN[&I];
        auto &iOUT = OUT[&I];

        // if the value is not in the IN, skip
        if (iIN.find(lockbounds->id) == iIN.end()) {
          continue;
        }

        // in the IN

        // if the value is not in the OUT, lock after this instruction
        if (iOUT.find(lockbounds->id) == iOUT.end()) {
          lockbounds->unlocks.insert(I.getNextNode());
          continue;
        }

        // in the IN, also in the OUT

        // special case for branches: insert an unlock on the "exit edge" of a branch
        // if the branch has the value in the OUT, but the branched-to instruction does
        // not have it in the IN.
        if (auto *branch = dyn_cast<BranchInst>(&I)) {
          for (auto succ : branch->successors()) {
            auto succInst = &succ->front();
            auto &succIN = IN[succInst];
            // if it's not in the IN of succ, lock on the edge
            if (succIN.find(lockbounds->id) == succIN.end()) {
							// HACK: split the edge and insert on the branch
							BasicBlock *from = I.getParent();
							BasicBlock *to = succ;
							auto trampoline = llvm::SplitEdge(from, to);

              lockbounds->unlocks.insert(trampoline->getTerminator());
            }
          }
        }
      }
    }
  }
#endif


  // if (func.getName() == "sum")  // only print for certain functions
  {
    std::unordered_set<Node *> nodes;
    for (auto &root : this->roots) {
      extract_nodes(root.get(), nodes);
    }
    std::map<Value *, Node *> inst2node;
    for (auto &node : nodes) {
      inst2node[node->val] = node;
    }
    alaska::println("------------------- { Analysis for lock forest: ", func.getName(), " } -------------------");
    alaska::println("digraph {");
    alaska::println("  label=\"lock forest for ", func.getName(), "\";");
    alaska::println("  compound=true;");
    alaska::println("  graph[nodesep=1];");
    alaska::println("  node[fontsize=9,shape=none,style=filled,fillcolor=white];");


    auto end_row = "</td></tr>";

    for (auto &BB : func) {
      alaska::print("  n", &BB, " [label=<<table border=\"1\" cellspacing=\"0\" padding=\"0\">");

      // generate the label
      //
      alaska::print("<tr><td align=\"left\" port=\"header\" border=\"0\">basic block: ");
      BB.printAsOperand(errs(), false);
      alaska::print(end_row);
      for (auto &I : BB) {
        // before printing the instruction, insert any lock/unlock calls before this instruction
        for (auto &[lid, lock] : this->locks) {
          // if (lock->pointer == &I) {
          //   // alaska::print("\\l|");
          //   alaska::print(" src", lid);
          // }
          if (lock->lockBefore == &I) {
            alaska::print("<tr><td align=\"left\" border=\"0\" bgcolor=\"green\">");
            alaska::print("lock lid", lid, " (");
            lock->pointer->printAsOperand(errs(), false);
            alaska::print(")", end_row);
          }

          for (auto &unlock : lock->unlocks) {
            if (unlock == &I) {
              alaska::print("<tr><td align=\"left\" border=\"0\" bgcolor=\"orange\">");
              alaska::print("unlock lid", lid, " (");
              lock->pointer->printAsOperand(errs(), false);
              alaska::print(")", end_row);
            }
          }
        }
        alaska::print("<tr><td align=\"left\"  port=\"n", &I, "\" border=\"0\">", escape(I), "</td>");

        alaska::print("<td align=\"left\" border=\"0\">");



        auto dump_set = [&](const char *name, std::set<unsigned> &s) {
          if (s.empty()) return;
          errs() << "" << name << ":";
          for (auto v : s) {
            errs() << " " << v;
          }
          errs() << "  ";
        };


        dump_set("GEN", GEN[&I]);
        dump_set("KILL", KILL[&I]);
        dump_set("IN", IN[&I]);
        dump_set("OUT", OUT[&I]);

        alaska::print("</td>");
        alaska::print("</tr>");

        // auto *node = inst2node[&I];
        // alaska::print("\\l|");
        // if (node) {
        //   if (node->lock_id != UINT_MAX) {
        //     alaska::print(" use", node->lock_id);
        //   }
        // }
        //
        // // go through lock locations
        // for (auto &[lid, lock] : this->locks) {
        //   if (lock->pointer == &I) {
        //     // alaska::print("\\l|");
        //     alaska::print(" src", lid);
        //   }
        // }
      }


      alaska::print("</table>>");  // end label
      alaska::print("];\n");
    }


    for (auto &BB : func) {
      auto *term = BB.getTerminator();
      for (unsigned i = 0; i < term->getNumSuccessors(); i++) {
        auto *other = term->getSuccessor(i);
        alaska::println("  n", &BB, ":n", term, " -> n", other);
      }


      // for (auto &I : BB) {
      //   auto *node = inst2node[&I];
      //   if (node) {
      //     if (node->lock_id != UINT_MAX) {
      //       auto &lock = locks[node->lock_id];
      //       auto *lock_bb = lock->lockBefore->getParent();
      //
      //       alaska::println("  n", &BB, ":n", &I, " -> n", lock_bb, ":lock", node->lock_id, " [color=orange]");
      //     }
      //   }
      // }
    }


    alaska::println("}\n\n\n");
  }


  // delete df;


#ifdef ALASKA_DUMP_FLOW_FOREST
  if (false) dump_dot();
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
