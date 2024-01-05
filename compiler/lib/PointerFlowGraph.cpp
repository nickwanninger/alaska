#include <alaska/PointerFlowGraph.h>
#include <alaska/Translations.h>
#include <alaska/Utils.h>

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstVisitor.h"


static bool no_strict_alias(void) {
  static bool set = getenv("ALASKA_NO_STRICT_ALIAS") != NULL;
  return set;
}

struct NodeConstructionVisitor : public llvm::InstVisitor<NodeConstructionVisitor> {
  alaska::FlowNode &node;
  NodeConstructionVisitor(alaska::FlowNode &node)
      : node(node) {
  }

  void visitGetElementPtrInst(llvm::GetElementPtrInst &I) {
    if (no_strict_alias()) {
      node.type = alaska::Source;
      node.colors.insert(node.id);
    } else {
      auto &use = I.getOperandUse(0);
      node.add_in_edge(&use);
      node.type = alaska::Transient;
    }
  }

  void visitCastInst(llvm::CastInst &I) {
    node.type = alaska::Transient;

    if (auto p2i = dyn_cast<IntToPtrInst>(&I)) {
      node.type = alaska::Source;
      node.colors.insert(node.id);
    }
  }

  // void visitPHINode(llvm::PHINode &I) {
  //   node.type = alaska::Source;
  //   node.colors.insert(node.id);
  //   return;
  //
  //   node.type = alaska::Transient;
  //   for (auto &incoming : I.incoming_values()) {
  //     node.add_in_edge(&incoming);
  //   }
  // }

  void visitAlloca(llvm::AllocaInst &I) {
    node.type = alaska::Source;
    // alloca has no color. it is not an allocation we care about.
  }

  // all else
  void visitInstruction(llvm::Instruction &I) {
    node.type = alaska::Source;
    node.colors.insert(node.id);
  }
};


alaska::FlowNode::FlowNode(alaska::PointerFlowGraph &graph, llvm::Value *value)
    : graph(graph)
    , value(value) {
  id = graph.next_id++;
}

void alaska::FlowNode::populate_edges(void) {
  if (auto I = dyn_cast<llvm::Instruction>(value)) {
    NodeConstructionVisitor vis(*this);
    vis.visit(I);
  } else {
    type = NodeType::Source;
    if (auto glob = dyn_cast<llvm::GlobalVariable>(value)) {
      // globals are not allocations we care about, thus they have no color
    } else {
      colors.insert(id);
    }
  }
}


std::set<alaska::FlowNode *> alaska::FlowNode::get_in_nodes(void) const {
  std::set<alaska::FlowNode *> out;
  for (auto e : in) {
    out.insert(&graph.get_node(e->get()));
  }
  return out;
}


std::set<alaska::FlowNode *> alaska::FlowNode::get_out_nodes(void) const {
  std::set<alaska::FlowNode *> outNodes;
  for (auto e : out) {
    outNodes.insert(&graph.get_node_including_sinks(e->getUser()));
  }
  return outNodes;
}

std::set<alaska::FlowNode *> alaska::FlowNode::get_dominated(llvm::DominatorTree &DT) const {
  std::set<alaska::FlowNode *> dominated;
  for (const Use *use : this->out) {
    auto &v = graph.get_node_including_sinks(use->getUser());
    auto I = dyn_cast<Instruction>(v.value);
    if (I && DT.dominates(use->get(), I)) {
      dominated.insert(&v);
    }
  }
  return dominated;
}



std::set<alaska::FlowNode *> alaska::FlowNode::get_dominators(llvm::DominatorTree &DT) const {
  std::set<alaska::FlowNode *> dominators;
  for (const Use *use : this->in) {
    auto &v = graph.get_node(use->get());
    auto I = dyn_cast<Instruction>(value);
    if (I && DT.dominates(use->get(), I)) {
      dominators.insert(&v);
    }
  }
  return dominators;
}


std::set<alaska::FlowNode *> alaska::FlowNode::get_postdominated(
    llvm::PostDominatorTree &PDT) const {
  std::set<alaska::FlowNode *> dominators;
  for (const Use *use : this->in) {
    auto &v = graph.get_node(use->get());
    auto I = dyn_cast<Instruction>(value);
    auto I2 = dyn_cast<Instruction>(use->get());
    if (I && I2 && PDT.dominates(I, I2)) {
      dominators.insert(&v);
    }
  }
  return dominators;
}




void alaska::FlowNode::add_in_edge(llvm::Use *use) {
  auto val = use->get();
  auto &other = graph.get_node(val);
  other.out.insert(use);
  in.insert(use);
}


void alaska::FlowNode::remove_in_edge(llvm::Use *use) {
  auto val = use->get();
  auto &other = graph.get_node(val);
  other.out.erase(use);
  in.erase(use);
}

alaska::PointerFlowGraph::PointerFlowGraph(llvm::Function &func)
    : m_func(func) {
  //
  // Step 1. Find all the sinks in the function.
  //
  // A mapping from the sink instruction to it's pointer Use. This is used to
  // later "seed" the graph by populating the 'in' edges of the sinks
  std::map<Instruction *, Use *> sinks;
  for (auto &BB : func) {
    for (auto &I : BB) {
      // Only loads and stores are sinks
      if (auto *load = dyn_cast<LoadInst>(&I)) {
        sinks[load] = &load->getOperandUse(0);
      } else if (auto *store = dyn_cast<StoreInst>(&I)) {
        sinks[store] = &store->getOperandUse(1);
      }
    }
  }


  // Create the graph by calling get_node() on the sink values. This will
  // recurse and compute the entire graph from the bottom up
  for (auto &[sink, use] : sinks) {
    // allocate the sink in the m_sinks set, which will not compute edges
    m_sinks[sink] = std::make_unique<alaska::FlowNode>(*this, sink);
    auto &node = m_sinks[sink];
    node->type = alaska::Sink;
    node->add_in_edge(use);
  }


  // gather the nodes into a single set
  auto nodes = get_all_nodes();


  bool phi_patched;
  do {
    phi_patched = false;
    // Clear the colors
    // for (auto node : nodes) {
    //   if (node->type == alaska::Source) {
    //     continue;
    //   }
    //   node->colors.clear();
    // }

    // Compute the colors of each node by flowing them down from the sources
    while (true) {
      bool changed = false;
      for (auto node : nodes) {
        if (node->type == alaska::Source) {
          continue;
        }
        auto old = node->colors;
        node->colors.clear();
        for (auto in : node->get_in_nodes()) {
          for (auto color : in->colors) {
            node->colors.insert(color);
          }
        }
        if (node->colors != old) changed |= true;
      }
      if (!changed) break;
    }

    // for (auto node : nodes) {
    //   if (node->type != alaska::Transient) {
    //     continue;
    //   }
    //   if (auto *phi = dyn_cast<PHINode>(node->value)) {
    //     if (node->colors.size() > 1) {
    //       phi_patched = true;
    //       node->type = alaska::Source;
    //       node->colors.clear();
    //       node->colors.insert(node->id);
    //       for (auto edge : node->in) {
    //         node->remove_in_edge(edge);
    //       }
    //     }
    //   }
    // }
  } while (phi_patched);

#ifdef ALASKA_DUMP_FLOW_GRAPH
  alaska::println("--------------------------------------");
  llvm::PostDominatorTree PDT(func);
  llvm::DominatorTree DT(func);
  dump_dot(DT, PDT);
  alaska::println("--------------------------------------");
#endif
}


alaska::FlowNode &alaska::PointerFlowGraph::get_node_including_sinks(llvm::Value *val) {
  if (m_sinks.find(val) != m_sinks.end()) {
    return *m_sinks[val].get();
  }
  return get_node(val);
}

alaska::FlowNode &alaska::PointerFlowGraph::get_node(llvm::Value *val) {
  if (m_nodes.find(val) == m_nodes.end()) {
    m_nodes[val] = std::make_unique<FlowNode>(*this, val);
    m_nodes[val]->populate_edges();
  }
  return *m_nodes[val].get();
}


std::set<alaska::FlowNode *> alaska::PointerFlowGraph::get_nodes(void) const {
  std::set<alaska::FlowNode *> nodes;
  for (auto &[value, node] : m_sinks) {
    if (node->colors.size() == 0) continue;
    nodes.insert(node.get());
  }
  for (auto &[value, node] : m_nodes) {
    if (node->colors.size() == 0) continue;
    nodes.insert(node.get());
  }
  return nodes;
}

std::set<alaska::FlowNode *> alaska::PointerFlowGraph::get_all_nodes(void) const {
  std::set<alaska::FlowNode *> nodes;
  for (auto &[value, node] : m_sinks) {
    nodes.insert(node.get());
  }
  for (auto &[value, node] : m_nodes) {
    nodes.insert(node.get());
  }
  return nodes;
}



void alaska::PointerFlowGraph::dump_dot(
    llvm::DominatorTree &DT, llvm::PostDominatorTree &PDT) const {
  auto nodes = get_nodes();

  alaska::println("digraph {");
  alaska::println("  label=\"", m_func.getName(), "\";");
  alaska::println("  compound=true;");
  alaska::println("  start=1;");


  std::map<llvm::BasicBlock *, std::set<alaska::FlowNode *>> bb_nodes;

  auto emitNodeDef = [&](alaska::FlowNode *node, const char *indent = "") {
    const char *color = NULL;
    switch (node->type) {
      case alaska::Source:
        // blue
        color = "#aec9fc70";
        break;
      case alaska::Sink:
        // red
        color = "#dc5d4a70";
        break;
      case alaska::Transient:
        // gray
        color = "#dedcdb70";
        break;
    }

    std::string color_label = "colors:";
    for (auto color : node->colors) {
      color_label += " ";
      color_label += std::to_string(color);
    }
    errs() << indent << "  n" << node->id << " [label=\"";
    // node->value->printAsOperand(errs(), false);
    errs() << *node->value;
    errs() << "\\n" << color_label << "\"";
    errs() << ", shape=box";
    errs() << ", style=filled";
    errs() << ", fillcolor=\"" << color << "\"";
    errs() << "];\n";
  };

  auto emitNodeEdges = [&](alaska::FlowNode *node, const char *indent = "") {
    for (Use *use : node->out) {
      auto &v = node->graph.get_node_including_sinks(use->getUser());
      alaska::println(indent, "  n", node->id, " -> n", v.id, "[color=black,style=dashed];");
    }
    // for (auto *dominated : node->get_dominated(DT)) {
    //   alaska::println(indent, "  n", node->id, " -> n", dominated->id, "[color=red];");
    // }
    // for (auto *dominated : node->get_postdominated(PDT)) {
    //   alaska::println(indent, "  n", node->id, " -> n", dominated->id, "[color=blue];");
    // }
  };


  for (auto *node : nodes) {
    if (auto I = dyn_cast<Instruction>(node->value)) {
      bb_nodes[I->getParent()].insert(node);
    } else {
      emitNodeDef(node);
      emitNodeEdges(node);
    }
  }

  for (auto &[bb, block_nodes] : bb_nodes) {
    errs() << "  subgraph \"cluster_";
    bb->printAsOperand(errs(), false);
    errs() << "\" {\n";
    errs() << "    style=filled;\n";
    errs() << "    bgcolor=\"#eff0f7\";\n";
    errs() << "    label=\"block ";
    bb->printAsOperand(errs(), false);
    errs() << "\";\n";

    for (auto *node : block_nodes) {
      emitNodeDef(node, "  ");
      emitNodeEdges(node, "  ");
    }

    errs() << "  }\n";
  }

  alaska::println("}");
}
