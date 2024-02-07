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

  llvm::Instruction *intro = nullptr;

  for (auto &BB : *func) {
    for (auto &I : BB) {
      if (intro == nullptr) intro = &I;
      this->graph.add_node_with_prop(&I, NodeProp());
    }
  }


  for (auto *inst : this->graph) {
    if (auto next = inst->getNextNode()) {
      Edge prop;
      if (auto loadInst = dyn_cast<LoadInst>(inst))
        prop.accessedValue = loadInst->getPointerOperand();
      if (auto storeInst = dyn_cast<StoreInst>(inst))
        prop.accessedValue = storeInst->getPointerOperand();
      this->graph.add_edge_with_prop(inst, next, prop);
    } else {
      auto bb = inst->getParent();
      for (auto succ : successors(bb)) {
        this->graph.add_edge_with_prop(inst, &succ->front(), Edge() /* Empty edge */);
      }
    }
  }



  bool changed;
  std::vector<llvm::Instruction *> toRemove;
  do {
    changed = false;
    for (auto *inst : this->graph) {
      int inc = this->graph.count_in_neighbors(inst);
      int outc = this->graph.count_out_neighbors(inst);
      if (inc != 1 || outc != 1) continue;

      // Grab the first incoming and outgoing (we are sure they exist)
      auto [in_node, in_edge] = *this->graph.incoming(inst).begin();
      auto [out_node, out_edge] = *this->graph.outgoing(inst).begin();

      auto inp = in_edge.prop();
      auto outp = out_edge.prop();

      if (not outp) {
        // cut the node out of the graph.
        changed = true;
        this->graph.add_edge_with_prop(in_node, out_node, inp);
        toRemove.push_back(inst);
      }
    }


    for (auto inst : toRemove) {
      this->graph.remove_nodes(inst);
    }
    toRemove.clear();  // empty the vector.
  } while (changed);



}


void alaska::AccessAutomata::dump(void) {
  auto [start, end] = get_start_end();


  alaska::println("digraph {");
  alaska::println("   compound = true;");


  auto L = graph.breadth_first_order(start);
  for (auto node : L) {
    // for (auto &node : this->graph) {
    alaska::print("   n", node, " [");
    alaska::print("label=\"", "\"");

    if (node == start) {
      alaska::print(",shape=box");
    } else if (node == end) {
      alaska::print(",shape=doublecircle");
    } else {
      alaska::print(",shape=circle");
    }


    alaska::println("];");
  }
  for (auto &from : this->graph) {
    for (auto &[to, edge] : this->graph.outgoing(from)) {
      alaska::print("   n", from, " -> n", to, " [label=\"");

      if (edge.prop()) {
        edge.prop().accessedValue->printAsOperand(llvm::errs(), true);
      }
      alaska::println("\", shape=box];");
    }
  }
  alaska::println("}\n");
}



std::pair<llvm::Instruction *, llvm::Instruction *> alaska::AccessAutomata::get_start_end(void) {
  llvm::Instruction *start = nullptr;
  llvm::Instruction *end = nullptr;



  for (auto &node : this->graph) {
    if (this->graph.count_in_neighbors(node) == 0) {
      ALASKA_SANITY(start == nullptr, "Cannot be multiple start nodes in the graph");
      start = node;
    }

    if (this->graph.count_out_neighbors(node) == 0) {
      ALASKA_SANITY(end == nullptr, "Cannot be multiple end nodes in the graph");
      end = node;
    }
  }

  return std::make_pair(start, end);
}
