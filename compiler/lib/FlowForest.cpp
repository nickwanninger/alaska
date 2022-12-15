#include <FlowForest.h>
#include <Graph.h>
#include <Utils.h>
#include <deque>
#include <unordered_set>
#include "llvm/IR/Instructions.h"




llvm::Instruction *alaska::FlowForest::Node::effective_instruction(void) {
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

alaska::FlowForest::Node::Node(alaska::FlowNode *val, alaska::FlowForest::Node *parent) {
  this->val = val->value;
  this->parent = parent;
  for (auto *out : val->get_out_nodes()) {
    children.push_back(std::make_unique<Node>(out, this));
  }
}




alaska::FlowForest::FlowForest(alaska::PointerFlowGraph &G, llvm::PostDominatorTree &PDT) : func(G.func()) {
  // The nodes which have no in edges that it post dominates
  std::unordered_set<alaska::FlowNode *> roots;
  // "which nodes' in edges should we look at next"
  std::deque<alaska::FlowNode *> worklist;
  for (auto *node : G.get_nodes()) {
    if (node->type == alaska::NodeType::Sink) worklist.push_back(node);
  }

  while (!worklist.empty()) {
    auto *current_node = worklist.front();
    worklist.pop_front();
    auto *current_inst = dyn_cast<Instruction>(current_node->value);
    if (current_inst == NULL) {
      roots.insert(current_node);
      continue;
    }

    auto in_nodes = current_node->get_in_nodes();

    // If there is nothing in the in edges, this node is a root.
    // This is kindof a hack to deal with the fact that alloca pointers
    // are pruned from the flow graph. If we don't do this, -O0 programs
    // might not have *any* trees in the forest.
    if (in_nodes.size() == 0) {
      roots.insert(current_node);
    } else {
      for (auto *in : in_nodes) {
        bool postdominates = false;
        if (auto in_arg = dyn_cast<llvm::Argument>(in->value)) {
          auto &F = G.func();
          auto *in_bb = &F.getEntryBlock();
          auto *current_bb = current_inst->getParent();

          if (PDT.dominates(current_bb, in_bb)) {
            postdominates = true;
          }
        } else if (auto in_inst = dyn_cast<llvm::Instruction>(in->value)) {
          if (PDT.dominates(current_inst, in_inst)) {
            postdominates = true;
          }
        }

        if (postdominates) {
          // If the in edge is post dominated by the current node, insert it into
          // the work list.
          worklist.push_back(in);
        } else {
          // If the in edge is *not* post dominated by the current
          // node, the current node is a root
          roots.insert(current_node);
        }
      }
    }
  }
  alaska::println("roots: ", roots.size());

  for (auto *root : roots) {
    this->roots.push_back(std::make_unique<Node>(root));
  }


#ifdef ALASKA_DUMP_FLOW_FOREST
  dump_dot();
#endif
}


static void extract_nodes(alaska::FlowForest::Node *node, std::unordered_set<alaska::FlowForest::Node *> &nodes) {
  nodes.insert(node);

  for (auto &child : node->children) {
    extract_nodes(child.get(), nodes);
  }
}

void alaska::FlowForest::dump_dot(void) {
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
    errs() << *node->val;
    errs() << "\", shape=box";
    errs() << ", style=filled";
    errs() << "];\n";
  }

  for (auto node : nodes) {
    if (node->parent) {
      errs() << "  n" << node->parent << " -> n" << node << "\n";
    }
  }
  alaska::println("}\n\n\n");
}