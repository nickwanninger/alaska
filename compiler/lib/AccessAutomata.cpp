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


// TODO: use the edges of a PHI node in a pointer flow graph as a way
//       of encoding "recurse access from" when dealing with PHI node
//       based linked list walking



// Assuming you have a GetElementPtrInst* named gepInst
static bool isFirstOffsetZero(GetElementPtrInst *gepInst) {
  if (gepInst->getNumOperands() < 2) return false;
  Value *firstIndexOperand = gepInst->getOperand(1);
  if (ConstantInt *constantIndex = dyn_cast<ConstantInt>(firstIndexOperand)) {
    return constantIndex->isZero();
  }
  return false;
}


static std::map<llvm::Instruction *, std::set<long>> memory_access_offsets(llvm::Value *obj) {
  alaska::DirectedGraph<llvm::Value *, std::set<long>> g;

  std::stack<llvm::Value *> q;
  std::set<long> empty;
  auto add = [&](llvm::Value *v) -> void {
    if (not g.has_node(v)) {
      q.push(v);
      g.add_nodes(v);
    }
  };

  add(obj);

  // Construct the use-def graph without labeling their edges.
  while (not q.empty()) {
    auto v = q.top();
    q.pop();

    for (auto user : v->users()) {
      if (auto gep = dyn_cast<GetElementPtrInst>(user)) {
        if (isFirstOffsetZero(gep)) {
          add(user);
          g.add_edge_with_prop(v, user, empty);
        }
      } else if (isa<PHINode>(user)) {
        add(user);
        g.add_edge_with_prop(v, user, empty);
      } else if (isa<LoadInst>(user) or isa<StoreInst>(user)) {
        g.add_nodes(dyn_cast<llvm::Value>(user));
        g.add_edge_with_prop(v, user, empty);
      }
    }
  }


  // Condense the graph to eliminate cycles and produce a DAG
  // TODO: Validate that this makes sense.
  auto con = g.condense();

  // Compute the topological order in which to flow offsets
  auto order_o = con.topological_order();
  ALASKA_SANITY(order_o.has_value(), "A condensed graph must have a topo order");
  auto order = order_o.value();


  // Iterate over that order, flowing incoming offests to the outgoing edges w/ some
  // node-specific transformation.
  for (auto *node : order) {
    std::set<long> all_incoming;
    // if there are no incoming edges, insert an offset of 0
    if (con.count_in_neighbors(node) == 0) {
      all_incoming.insert(0);
    } else {
      for (auto &[_, e] : con.incoming(node)) {
        all_incoming.insert(e.prop().begin(), e.prop().end());
      }
    }
    // Apply node-specific transformations
    if (auto gep = dyn_cast<llvm::GetElementPtrInst>(node)) {
      llvm::APInt offset(64, 0, true);

      auto DL = gep->getModule()->getDataLayout();

      if (gep->accumulateConstantOffset(DL, offset)) {
        int signedOffset = offset.getSExtValue();
        std::set<long> transformed;
        for (auto &e : all_incoming) {
          transformed.insert(e + signedOffset);
        }

        all_incoming = std::move(transformed);
      } else {
        // If we could not accumulate a constant offset, wipe out the all_incoming.
        all_incoming.clear();
      }
    }

    for (auto &[_, e] : con.outgoing(node)) {
      // Flow that incoming to the out edges.
      e.prop().insert(all_incoming.begin(), all_incoming.end());
    }
  }


  // alaska::println("Flow graph:");
  // alaska::println("digraph {");
  // alaska::println("  compound = true;");
  //
  // for (auto *v : order) {
  //   alaska::println("  n", v, "[label=\"", *v, "\"]");
  // }
  //
  // for (auto &from : order) {
  //   for (auto &[to, edge] : con.outgoing(from)) {
  //     alaska::print("   n", from, " -> n", to, " [label=\"");
  //     for (auto e : edge.prop()) {
  //       alaska::print(" ", e);
  //     }
  //     alaska::println("\", shape=box];");
  //   }
  // }
  // alaska::println("}");

  std::map<llvm::Instruction *, std::set<long>> res;

  for (auto *inst : order) {
    if (isa<LoadInst>(inst) or isa<StoreInst>(inst)) {
      std::set<long> offsets;
      for (auto &[_, e] : con.incoming(inst)) {
        offsets.insert(e.prop().begin(), e.prop().end());
      }
      res[dyn_cast<Instruction>(inst)] = std::move(offsets);
    }
  }
  return res;
}



alaska::AccessAutomata::AccessAutomata(llvm::Value *object, alaska::OptimisticTypes &OT) {
  using namespace alaska::re;
  llvm::Function *func = nullptr;
  if (auto arg = dyn_cast<llvm::Argument>(object)) {
    func = arg->getParent();
  }

  if (auto inst = dyn_cast<llvm::Instruction>(object)) {
    func = inst->getFunction();
  }

  ALASKA_SANITY(func != NULL, "Invalid object to construct Automata from");

  llvm::Instruction *intro = nullptr;

  int id = 0;
  for (auto &BB : *func) {
    for (auto &I : BB) {
      if (intro == nullptr) intro = &I;
      this->graph.add_node_with_prop(&I, NodeProp());
      this->graph.node_prop(&I).id = id++;
    }
  }


  for (auto *inst : this->graph) {
    if (auto next = inst->getNextNode()) {
      Edge prop;
      if (auto loadInst = dyn_cast<LoadInst>(inst))
        prop.accessedValue = loadInst->getPointerOperand();
      if (auto storeInst = dyn_cast<StoreInst>(inst))
        prop.accessedValue = storeInst->getPointerOperand();
      this->graph.add_edge_with_prop(inst, next, prop ? tok<Edge>(std::move(prop)) : nullptr);
    } else {
      auto bb = inst->getParent();
      for (auto succ : successors(bb)) {
        this->graph.add_edge_with_prop(inst, &succ->front(), nullptr /* Empty edge */);
      }
    }
  }


  auto sequentialize = [](ExprPtr<Edge> a, ExprPtr<Edge> b) {
    // If both are defined, create an alternative
    if (a.get() and b.get()) return alaska::re::seq<Edge>({a, b});
    return a ? a : b;
  };

  auto alternative = [](ExprPtr<Edge> a, ExprPtr<Edge> b) {
    // If both are defined, create an alternative
    if (a and b) {
      if (a->equal(b.get())) return a;
      return alaska::re::alt<Edge>({a, b});
    }

    return a ? a : b;
  };



  bool changed;
  bool finished_reduction = false;
  std::vector<llvm::Instruction *> toRemove;
  do {
    dump();
    changed = false;
    for (auto *inst : this->graph) {
      int inc = this->graph.count_in_neighbors(inst);
      int outc = this->graph.count_out_neighbors(inst);

      // XXX: Safety measure for now.
      ExprPtr<Edge> self_edge = nullptr;
      for (auto &[in, e] : this->graph.incoming(inst)) {
        if (in == inst) {
          self_edge = e.prop();
          break;
        }
      }

      if (not finished_reduction and self_edge) continue;

      if (((self_edge and inc == 2) or (not self_edge and inc == 1)) and outc >= 1) {
        llvm::Instruction *in_node = nullptr;
        ExprPtr<Edge> in_edge = nullptr;

        for (auto &[n, e] : this->graph.incoming(inst)) {
          if (n != inst) {
            in_edge = e.prop();
            in_node = n;
          }
        }

        if (self_edge) {
          in_edge = sequentialize(in_edge, alaska::re::star<Edge>(std::move(self_edge)));
        }

        // Create a new sequence edge between in_node, and each outgoing node.
        for (auto &[out_node, out_edge] : this->graph.outgoing(inst)) {
          if (out_node == inst) continue;  // self edge handling
          auto newEdge = sequentialize(in_edge, out_edge.prop());

          // if there is already an edge between these two nodes, alternative it.
          if (this->graph.count_edges(in_node, out_node)) {
            auto e = this->graph.get_edge(in_node, out_node);
            this->graph.remove_edge(in_node, out_node);
            newEdge = alternative(newEdge, e);
          }
          this->graph.add_edge_with_prop(in_node, out_node, newEdge);
        }


        if (self_edge) {
          this->graph.remove_edge(inst, inst);
        }

        toRemove.push_back(inst);
      }

      if (toRemove.size() > 0) break;
    }


    for (auto inst : toRemove) {
      // alaska::println("Removing node ", this->graph.node_prop(inst).id);
      this->graph.remove_nodes(inst);
      changed = true;
    }


    toRemove.clear();  // empty the vector.

    if (not changed and not finished_reduction) {
      changed = true;
      finished_reduction = true;
    }
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
    alaska::print("label=\"", this->graph.node_prop(node).id, "\"");

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
      if (edge.prop()) llvm::errs() << *edge.prop();

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
      // ALASKA_SANITY(start == nullptr, "Cannot be multiple start nodes in the graph");
      start = node;
    }

    if (this->graph.count_out_neighbors(node) == 0) {
      // ALASKA_SANITY(end == nullptr, "Cannot be multiple end nodes in the graph");
      end = node;
    }
  }

  return std::make_pair(start, end);
}
