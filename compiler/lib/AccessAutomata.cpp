/*
 * This file is part of the Alaska Handle-Based Memory Management System
 *
 * Copyright (c) 2024, Nick Wanninger <ncw@u.northwestern.edu>
 * Copyright (c) 2024, The Constellation Project
 * All rights reserved.
 *
 * This is free software.  You are permitted to use, redistribute,
 * and modify it as specified in the file "LICENSE".
 */

#include <alaska/AccessAutomata.h>
#include <alaska/Graph.h>

alaska::AccessAutomata::AccessAutomata(llvm::Value *object, alaska::OptimisticTypes &OT) {
  llvm::Function *func = nullptr;
  if (auto arg = dyn_cast<llvm::Argument>(object)) {
    func = arg->getParent();
  }

  if (auto arg = dyn_cast<llvm::Argument>(object)) {
    func = arg->getParent();
  }

  ALASKA_SANITY(func != NULL, "Invalid object to construct Automata from");

  alaska::DirectedGraph<llvm::Instruction *, llvm::Value *> g;

  llvm::Instruction *intro = nullptr;

  for (auto &BB : *func) {
    for (auto &I : BB) {
      if (intro == nullptr) intro = &I;
      g.add_nodes(&I);
    }
  }


  for (auto *inst : g) {
    if (auto next = inst->getNextNode()) {
      llvm::Value *prop = 0;
      if (auto loadInst = dyn_cast<LoadInst>(inst)) prop = loadInst->getPointerOperand();
      if (auto storeInst = dyn_cast<StoreInst>(inst)) prop = storeInst->getPointerOperand();
      g.add_edge_with_prop(inst, next, prop);
    } else {
      auto bb = inst->getParent();
      for (auto succ : successors(bb)) {
        g.add_edge_with_prop(inst, &succ->front(), nullptr);
      }
    }
  }



  bool changed;
  std::vector<llvm::Instruction *> toRemove;
  do {
    changed = false;
    for (auto *inst : g) {
      int inc = g.count_in_neighbors(inst);
      int outc = g.count_out_neighbors(inst);
      if (inc != 1 || outc != 1) continue;

      // Grab the first incoming and outgoing (we are sure they exist)
      auto [in_node, in_edge] = *g.incoming(inst).begin();
      auto [out_node, out_edge] = *g.outgoing(inst).begin();

      auto inp = in_edge.prop();
      auto outp = out_edge.prop();

      if (outp == NULL) {
        // cut the node out of the graph.
        changed = true;
        g.add_edge_with_prop(in_node, out_node, inp);
        toRemove.push_back(inst);
      }
    }


    for (auto inst : toRemove) {
      g.remove_nodes(inst);
    }
    toRemove.clear();  // empty the vector.
  } while (changed);


  alaska::println("digraph {");
  alaska::println("   compound = true;");
  for (auto &node : g)
    alaska::println("   n", node, " [label=\"", "\",shape=box];");
  for (auto &from : g) {
    for (auto &[to, edge] : g.outgoing(from)) {
      alaska::print("   n", from, " -> n", to, " [label=\"");
      if (edge.prop()) {
        edge.prop()->printAsOperand(llvm::errs(), true);
      }
      alaska::println("\", shape=box];");
    }
  }
  alaska::println("}\n");
}



alaska::AccessAutomata::State *alaska::AccessAutomata::get_state(llvm::Instruction &I) {
  auto found = this->states.find(&I);

  if (found == this->states.end()) {
    // make one!
    auto state = new State();
    state->inst = &I;
    states[&I] = std::unique_ptr<State>(state);
    return state;
  }

  return found->second.get();
}


void alaska::AccessAutomata::dump(void) {}
