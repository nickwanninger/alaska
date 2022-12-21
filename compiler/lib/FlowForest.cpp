#include <FlowForest.h>
#include <Graph.h>
#include <Utils.h>
#include <deque>
#include <unordered_set>
#include "llvm/IR/Instructions.h"


static void extract_nodes(alaska::FlowForest::Node *node, std::unordered_set<alaska::FlowForest::Node *> &nodes) {
  nodes.insert(node);

  for (auto &child : node->children) {
    extract_nodes(child.get(), nodes);
  }
}

int alaska::FlowForest::Node::depth(void) {
  if (parent == NULL) return 0;
  return parent->depth() + 1;
}

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
  llvm::DominatorTree DT(func);
  // The nodes which have no in edges that it post dominates
  std::unordered_set<alaska::FlowNode *> roots;
  // all the sources are roots in the flow tree
  for (auto *node : G.get_nodes()) {
    if (node->type == alaska::NodeType::Source) roots.insert(node);
  }

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

#ifdef ALASKA_DUMP_FLOW_FOREST
  dump_dot();
#endif
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
    errs() << "{" << *node->val;
    if (node->parent == NULL && node->children.size() > 0) {
      errs() << "|{";
      int i = 0;

      for (auto &child : node->children) {
        if (i++ != 0) {
          errs() << "|";
        }

        errs() << "<n" << child.get() << ">";
        if (child->share_lock_with == NULL) {
          errs() << "lock";
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
      errs() << "  n" << node->parent << ":n" << node << " -> n" << node << "\n";

      if (node->parent->parent == NULL && node->share_lock_with) {
        errs() << "  n" << node->parent << ":n" << node->compute_shared_lock() << " -> n" << node
               << " [style=\"dashed\", color=orange]\n";
      }
    }


    // for (auto *dom : node->dominates) {
    //   errs() << "  n" << node << " -> n" << dom << " [color=red, label=\"D\", style=\"dashed\"]\n";
    // }

    // for (auto *dom : node->postdominates) {
    //   errs() << "  n" << node << " -> n" << dom << " [color=blue, label=\"PD\"]\n";
    // }
  }


  alaska::println("}\n\n\n");
}
