#include <LockForest.h>
#include <Graph.h>
#include <Utils.h>
#include <deque>
#include <unordered_set>
#include <sstream>
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include <noelle/core/DataFlow.hpp>



// Either get the `incoming_lock` of this node or the node it shares with, or the parent's translated
llvm::Instruction *get_incoming_translated_value(alaska::LockForest::Node &node) {
  if (node.incoming_lock != NULL) return node.incoming_lock;

  if (node.parent->parent == NULL) {
    if (auto *shared = node.compute_shared_lock()) {
      ALASKA_SANITY(shared->incoming_lock != NULL, "Incoming lock on shared lock owner is null");
      return shared->incoming_lock;
    }
  }

  ALASKA_SANITY(node.parent->translated != NULL, "Parent of node does not have a translated value");
  return node.parent->translated;
}

struct TranslationVisitor : public llvm::InstVisitor<TranslationVisitor> {
  alaska::LockForest::Node &node;
  TranslationVisitor(alaska::LockForest::Node &node) : node(node) {}

  void visitGetElementPtrInst(llvm::GetElementPtrInst &I) {
    // create a new GEP right after this one.
    IRBuilder<> b(I.getNextNode());

    auto t = get_incoming_translated_value(node);
    std::vector<llvm::Value *> inds(I.idx_begin(), I.idx_end());
    node.translated = dyn_cast<Instruction>(b.CreateGEP(I.getSourceElementType(), t, inds, "", I.isInBounds()));
  }

  void visitLoadInst(llvm::LoadInst &I) {
    auto t = get_incoming_translated_value(node);
    I.setOperand(0, t);
  }
  void visitStoreInst(llvm::StoreInst &I) {
    auto t = get_incoming_translated_value(node);
    I.setOperand(1, t);
  }

  void visitInstruction(llvm::Instruction &I) {
    alaska::println("dunno how to handle this: ", I);
    ALASKA_SANITY(node.translated != NULL, "Dunno how to handle this node");
  }
};

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



alaska::LockForest::LockForest(llvm::Function &F) : func(F) {}


void alaska::LockForest::apply(alaska::LockForest::Node &node) {
  auto *inst = dyn_cast<llvm::Instruction>(node.val);
  // apply transformation
  TranslationVisitor vis(node);
  vis.visit(inst);

  // go over children and apply their transformation
  for (auto &child : node.children) {
    // go down to the children
    apply(*child);
  }
}

void alaska::LockForest::apply(void) {
  alaska::PointerFlowGraph G(func);
  // Compute the {,post}dominator trees and get loops
  llvm::DominatorTree DT(func);
  llvm::PostDominatorTree PDT(func);
  llvm::LoopInfo loops(DT);

  // The nodes which have no in edges that it post dominates
  std::unordered_set<alaska::FlowNode *> roots;

  // all the sources are roots in the pointer flow graph
  for (auto *node : G.get_nodes()) {
    if (node->type == alaska::NodeType::Source) roots.insert(node);
  }

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


  for (auto &root : this->roots) {
    for (auto &child : root->children) {
      auto *inst = dyn_cast<llvm::Instruction>(child->val);
      if (inst == NULL) {
        exit(EXIT_FAILURE);
      }
      // first, if the child does not share a lock with anyone, and
      // it's parent is a source, create the incoming lock
      if (child->share_lock_with == NULL && child->parent->parent == NULL) {
        auto &bounds = get_lockbounds(child->lock_id);
        child->incoming_lock = bounds.locked;
      }
    }
    for (auto &child : root->children) {
      apply(*child);
    }
  }

  // finally, insert unlocks
  for (auto &[id, bounds] : this->locks) {
    for (auto *position : bounds->unlocks) {
      if (auto phi = dyn_cast<PHINode>(position)) {
        position = phi->getParent()->getFirstNonPHI();
      }
      alaska::insertUnlockBefore(position, bounds->pointer);
    }
  }
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