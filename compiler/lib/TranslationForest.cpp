#include <alaska/TranslationForest.h>
#include <alaska/PointerFlowGraph.h>
#include <alaska/Utils.h>
#include <deque>
#include <set>
#include <sstream>
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/ADT/SparseBitVector.h"
#include <noelle/core/DataFlow.hpp>




// Either get the `incoming` of this node or the node it shares with, or the parent's
// translated
llvm::Instruction *get_incoming_translated_value(alaska::TranslationForest::Node &node) {
  if (node.incoming != NULL) return node.incoming;

  if (node.parent->parent == NULL) {
    if (auto *shared = node.compute_shared_translation()) {
      ALASKA_SANITY(
          shared->incoming != NULL, "Incoming translation on shared translation owner is null");
      return shared->incoming;
    }
  }

  ALASKA_SANITY(node.parent->translated != NULL, "Parent of node does not have a translated value");
  return node.parent->translated;
}

struct TranslationVisitor : public llvm::InstVisitor<TranslationVisitor> {
  alaska::TranslationForest::Node &node;
  TranslationVisitor(alaska::TranslationForest::Node &node)
      : node(node) {
  }

  void visitGetElementPtrInst(llvm::GetElementPtrInst &I) {
    // Simply insert an `alaska.derive` function right after `I`
    auto t = get_incoming_translated_value(node);
    node.translated = alaska::insertDerivedBefore(I.getNextNode(), t, &I);
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

static void extract_nodes(
    alaska::TranslationForest::Node *node, std::set<alaska::TranslationForest::Node *> &nodes) {
  nodes.insert(node);

  for (auto &child : node->children) {
    extract_nodes(child.get(), nodes);
  }
}

int alaska::TranslationForest::Node::depth(void) {
  if (parent == NULL) return 0;
  return parent->depth() + 1;
}

alaska::TranslationForest::Node *alaska::TranslationForest::Node::compute_shared_translation(void) {
  if (share_translation_with) {
    auto *s = share_translation_with->compute_shared_translation();
    if (s) return s;
    return share_translation_with;
  }
  return nullptr;
}

llvm::Instruction *alaska::TranslationForest::Node::effective_instruction(void) {
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

alaska::TranslationForest::Node::Node(
    alaska::FlowNode *val, alaska::TranslationForest::Node *parent) {
  this->val = val->value;
  this->parent = parent;
  for (auto *out : val->get_out_nodes()) {
    children.push_back(std::make_unique<Node>(out, this));
  }
}



static llvm::Loop *get_outermost_loop_for_translation(
    llvm::Loop *loopToCheck, llvm::Instruction *pointer, llvm::Instruction *user) {
  // return nullptr;
  if (loopToCheck->contains(user) && !loopToCheck->contains(pointer)) {
    return loopToCheck;
  }

  for (auto *subLoop : loopToCheck->getSubLoops()) {
    if (auto *loop = get_outermost_loop_for_translation(subLoop, pointer, user)) {
      return loop;
    }
  }
  return nullptr;
}


static llvm::Instruction *compute_translation_insertion_location(
    llvm::Value *pointerToTranslate, llvm::Instruction *user, llvm::LoopInfo &loops) {
  // the instruction to consider as the "location of the pointer". This is done for things like
  // arguments.
  llvm::Instruction *effectivePointerInstruction = NULL;


  if (auto invokeInst = dyn_cast<llvm::InvokeInst>(pointerToTranslate)) {
    effectivePointerInstruction = invokeInst->getNormalDest()->getFirstNonPHIOrDbg();
  } else if (auto pointerToTranslateInst = dyn_cast<llvm::Instruction>(pointerToTranslate)) {
    effectivePointerInstruction = pointerToTranslateInst;
  } else if (auto arg = dyn_cast<llvm::Argument>(pointerToTranslate)) {
    auto *func = arg->getParent();
    auto &in_bb = func->getEntryBlock();
    return in_bb.getFirstNonPHI();
  } else {
    // get the first instruction in the function the argument is a part of;
    effectivePointerInstruction = user->getParent()->getParent()->front().getFirstNonPHI();
  }
  ALASKA_SANITY(
      effectivePointerInstruction != NULL, "No effective instruction for pointerToTranslate");

  llvm::Loop *targetLoop = NULL;
  for (auto loop : loops) {
    targetLoop = get_outermost_loop_for_translation(loop, effectivePointerInstruction, user);
    if (targetLoop != NULL) break;
  }

  // If no loop was found, translate at the user.
  if (targetLoop == NULL) return user;

  llvm::BasicBlock *incoming, *back;
  targetLoop->getIncomingAndBackEdge(incoming, back);


  // errs() << "Found loop for " << *pointerToTranslate << ": " << *targetLoop;
  if (incoming && incoming->getTerminator()) {
    return incoming->getTerminator();
  }

  return user;
}



alaska::TranslationForest::TranslationForest(llvm::Function &F)
    : func(F) {
}




std::vector<std::unique_ptr<alaska::Translation>> alaska::TranslationForest::apply(void) {
  double start = alaska::time_ms();
  (void)start;
  alaska::PointerFlowGraph G(func);

  // Compute the {,post}dominator trees and get loops
  llvm::DominatorTree DT(func);
  llvm::PostDominatorTree PDT(func);
  llvm::LoopInfo loops(DT);

  // The nodes which have no in edges that it post dominates
  std::set<alaska::FlowNode *> temp_roots;

  // all the sources are roots in the pointer flow graph
  for (auto *node : G.get_nodes()) {
    if (node->type == alaska::NodeType::Source && alaska::shouldTranslate(node->value)) {
      temp_roots.insert(node);
    }
  }

  // printf("grab roots %lf\n", alaska::time_ms() - start);
  // start = alaska::time_ms();

  // Create the forest from the roots, and compute dominance relationships among top-level siblings
  for (auto *root : temp_roots) {
    auto node = std::make_unique<Node>(root);

    // compute which children dominate which siblings
    std::set<Node *> nodes;
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
            sibling->share_translation_with = child.get();
            child->dominates.insert(sibling.get());
          }
        }
      }
    }

    this->roots.push_back(std::move(node));
  }



  // printf("build forest %lf\n", alaska::time_ms() - start);
  // start = alaska::time_ms();

  for (auto &root : this->roots) {
    // errs() << "root " << root.get() << " val: " << *root->val << "\n";
    for (auto &child : root->children) {
      if (child->share_translation_with == NULL) {
        auto &lb = get_translation_bounds();
        lb.pointer = root->val;
        child->translation_id = lb.id;
      }
    }
    // Apply TranslationBounds to the forest nodes.
    for (auto &child : root->children) {
      unsigned id = child->translation_id;
      if (auto *share = child->compute_shared_translation()) {
        id = share->translation_id;
      }
      ALASKA_SANITY(id != UINT_MAX, "invalid translation id");
      child->set_translation_id(id);
    }
  }


  // printf("set translation id %lf\n", alaska::time_ms() - start);
  // start = alaska::time_ms();

  // Compute the instruction at which a translation should be inserted to minimize the
  // number of runtime invocations, but to also minimize the number of handles
  // which are translated when alaska_barrier functions are called. There are a few
  // decisions it this function makes:
  //   - if @user is inside a loop that @pointerToTranslation is not:
  //     - If the loop contains a call to `alaska_barrier`, translate before @user
  //     - else, translate before the branch into the loop's header.
  //   - else, translate at @user
  for (auto &root : this->roots) {
    for (auto &child : root->children) {
      auto *inst = dyn_cast<llvm::Instruction>(child->val);
      ALASKA_SANITY(inst != NULL, "child node has no instruction");
      // first, if the child does not share a translation with anyone, and
      // it's parent is a source, create the incoming translation
      if (child->share_translation_with == NULL && child->parent->parent == NULL) {
        auto &lb = get_translation_bounds(child->translation_id);
        // alaska::println("find:", *root->val);
        // alaska::println("inst:", *inst);
        lb.translateBefore = compute_translation_insertion_location(root->val, inst, loops);

        lb.pointer = alaska::insertRootBefore(lb.translateBefore, root->val);
        // errs() << *lb.pointer << "\n";
        IRBuilder<> b(lb.translateBefore);
      }
    }
  }

  // alaska::println(this->roots.size(), " roots");



  // printf("compute insertion %lf\n", alaska::time_ms() - start);
  // start = alaska::time_ms();

  ///////////////////////////////////////////////////////////////////////////
  //                      Compute release locations                        //
  ///////////////////////////////////////////////////////////////////////////

  std::map<Instruction *, TranslationBounds *> inst2bounds;
  {
    std::set<Node *> nodes;
    for (auto &root : this->roots) {
      for (auto &child : root->children) {
        extract_nodes(child.get(), nodes);
      }
    }

    for (auto &node : nodes) {
      if (auto *inst = dyn_cast<Instruction>(node->val)) {
        inst2bounds[inst] = &get_translation_bounds(node->translation_id);
      }
    }
    for (auto &[id, bounds] : translations) {
      inst2bounds[bounds->translateBefore] = bounds.get();
    }
  }

  // printf("compute release %lf\n", alaska::time_ms() - start);
  // start = alaska::time_ms();

  std::unordered_map<Instruction *, llvm::SparseBitVector<128>> GEN;
  std::unordered_map<Instruction *, llvm::SparseBitVector<128>> KILL;
  std::unordered_map<Instruction *, llvm::SparseBitVector<128>> IN;
  std::unordered_map<Instruction *, llvm::SparseBitVector<128>> OUT;

  std::set<Instruction *> todo;

  // Here, we do something horrible. we insert translation instructions just so we can do analysis.
  for (auto &[lid, tr] : translations) {
    tr->translated = insertTranslationBefore(tr->translateBefore, tr->pointer);
    // KILL[tr->translated].insert(lid);
    KILL[tr->translated].set(lid);
  }

  // initialize gen sets
  for (auto &BB : func) {
    for (auto &I : BB) {
      todo.insert(&I);
      auto f = inst2bounds.find(&I);
      if (f != inst2bounds.end()) {
        auto &lb = f->second;
        // GEN[&I].insert(lb->id);
        GEN[&I].set(lb->id);
      }
    }
  }


  // printf("gen set %lf\n", alaska::time_ms() - start);
  // start = alaska::time_ms();

  while (!todo.empty()) {
    // Grab some instruction from worklist...
    auto i = *todo.begin();
    // ...remove it from the worklist
    todo.erase(i);

    auto old_out = OUT[i];
    auto old_in = IN[i];


    // auto &gen = GEN[i];
    // auto &out = OUT[i];
    // IN = GEN U (OUT - KILL);
    IN[i] = GEN[i] | (OUT[i] - KILL[i]);

    OUT[i].clear();

    // successor updating
    if (auto *switch_inst = dyn_cast<SwitchInst>(i)) {
      for (auto caseIt : switch_inst->cases()) {
        OUT[i] |= IN[&caseIt.getCaseSuccessor()->front()];
      }
    } else if (auto *invoke_inst = dyn_cast<InvokeInst>(i)) {
      if (auto normal_bb = invoke_inst->getNormalDest()) {
        OUT[i] |= IN[&normal_bb->front()];
      }

      if (auto unwind_bb = invoke_inst->getUnwindDest()) {
        OUT[i] |= IN[&unwind_bb->front()];
      }
    } else if (auto *next_node = i->getNextNode()) {
      OUT[i] |= IN[next_node];
    } else {
      // end of a basic block, get all successor basic blocks
      for (auto *bb : successors(i)) {
        OUT[i] |= IN[&bb->front()];
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


  std::map<std::pair<llvm::BasicBlock *, llvm::BasicBlock *>, std::set<alaska::TranslationBounds *>>
      releaseTrampolines;

  // using the dataflow result, insert release locations into the corresponding translation bounds
  for (auto &BB : func) {
    for (auto &I : BB) {
      for (auto &[id, trbounds] : translations) {
        auto &iIN = IN[&I];
        auto &iOUT = OUT[&I];

        // if the value is not in the IN, skip
        if (!iIN.test(trbounds->id)) {
          continue;
        }

        // in the IN

        // if the value is not in the OUT, translate after this instruction
        if (!iOUT.test(trbounds->id)) {
          trbounds->releaseBefore.insert(I.getNextNode());
          continue;
        }

        // in the IN, also in the OUT

        // special case for branches: insert a release on the "exit edge" of a branch
        // if the branch has the value in the OUT, but the branched-to instruction does
        // not have it in the IN.
        if (auto *branch = dyn_cast<BranchInst>(&I)) {
          for (auto succ : branch->successors()) {
            auto succInst = &succ->front();
            auto &succIN = IN[succInst];
            // if it's not in the IN of succ, translate on the edge
            if (!succIN.test(trbounds->id)) {
              // HACK: split the edge and insert on the branch
              BasicBlock *from = I.getParent();
              BasicBlock *to = succ;
              // we cannot insert the trampoline split here, as we would be modifying the IR while
              // iterating it. Instead, record that we need to in the releaseTrampoline structure
              // above so we can insert the trampoline later.
              releaseTrampolines[std::make_pair(from, to)].insert(trbounds.get());
            }
          }
        }
      }
    }
  }

  for (auto &[edge, bounds] : releaseTrampolines) {
    auto from = edge.first;
    auto to = edge.second;
    auto trampoline = llvm::SplitEdge(from, to);

    for (auto b : bounds) {
      b->releaseBefore.insert(trampoline->getTerminator());
    }
  }

  std::vector<std::unique_ptr<alaska::Translation>> out;

  // convert the translation bounds structures to translations that we return.
  for (auto &[lid, tr] : translations) {
    auto l = std::make_unique<alaska::Translation>();
    l->translation = dyn_cast<CallInst>(tr->translated);
    for (auto *position : tr->releaseBefore) {
      if (position == NULL) {
        alaska::println("[WARNING] position was nulld in ", func.getName());
        continue;
      }
      if (auto phi = dyn_cast<PHINode>(position)) {
        position = phi->getParent()->getFirstNonPHI();
      }
      alaska::insertReleaseBefore(position, tr->pointer);
      l->releases.insert(dyn_cast<CallInst>(position->getPrevNode()));
    }
  }


  for (auto &root : this->roots) {
    for (auto &child : root->children) {
      auto *inst = dyn_cast<llvm::Instruction>(child->val);
      if (inst == NULL) {
        exit(EXIT_FAILURE);
      }
      // first, if the child does not share a translation with anyone, and
      // it's parent is a source, create the incoming translation
      if (child->share_translation_with == NULL && child->parent->parent == NULL) {
        auto &bounds = get_translation_bounds(child->translation_id);
        child->incoming = bounds.translated;
      }
    }

    for (auto &child : root->children) {
      apply(*child);
    }
  }

  // printf("conversion %lf\n", alaska::time_ms() - start);
  // start = alaska::time_ms();

  return out;
}


void alaska::TranslationForest::apply(alaska::TranslationForest::Node &node) {
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


alaska::TranslationBounds &alaska::TranslationForest::get_translation_bounds(unsigned id) {
  auto f = translations.find(id);
  ALASKA_SANITY(f != translations.end(), "translation not found\n");
  return *f->second;
}

alaska::TranslationBounds &alaska::TranslationForest::get_translation_bounds(void) {
  unsigned id = next_translation_id++;
  auto b = std::make_unique<TranslationBounds>();
  b->id = id;
  translations[id] = std::move(b);
  return get_translation_bounds(id);
}
