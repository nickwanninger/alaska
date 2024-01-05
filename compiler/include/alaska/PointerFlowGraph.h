#pragma once

#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/DerivedUser.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Mangler.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/LoopUtils.h"

#include <config.h>
#include <map>
#include <memory>  // shared_ptr
#include <set>

using namespace llvm;

namespace alaska {

  class PointerFlowGraph;

  enum NodeType {
    Source,     // Malloc, Realloc, Arguments, etc..
    Sink,       // Load, Store
    Transient,  // Gep, Phi, etc..
  };

  struct FlowNode {
    int id;
    // The value at this node. Typically an Instruction, but Arguments also occupy
    // a Node
    NodeType type;
    PointerFlowGraph &graph;
    llvm::Value *value = NULL;
    llvm::Value *pinned_value = NULL;  // HACK: abstraction leakage
    std::set<int> colors;

    FlowNode(PointerFlowGraph &graph, llvm::Value *value);
    void add_in_edge(llvm::Use *);
    void remove_in_edge(llvm::Use *);

    std::set<FlowNode *> get_in_nodes(void) const;
    std::set<FlowNode *> get_out_nodes(void) const;
    // get the nodes which this node dominates (out edges that it dominates)
    std::set<FlowNode *> get_dominated(llvm::DominatorTree &DT) const;
    // get the nodes which this node is dominated by (in edges that dominates
    // this)
    std::set<FlowNode *> get_dominators(llvm::DominatorTree &DT) const;

    // get the nodes which this node postdominates
    std::set<FlowNode *> get_postdominated(llvm::PostDominatorTree &PDT) const;

   protected:
    friend class PointerFlowGraph;

    // edges to other nodes
    std::set<llvm::Use *> in;
    std::set<llvm::Use *> out;
    void populate_edges(void);
  };

  // This structure is basically a PDG with *just* use-def chains and no memory dependencies.
  class PointerFlowGraph {
   public:
    PointerFlowGraph(llvm::Function &func);
    auto &func(void) {
      return m_func;
    }

    // get just the nodes that we care about (skip alloca and globals)
    std::set<alaska::FlowNode *> get_nodes(void) const;
    // get all nodes, including those we don't really care about.
    std::set<alaska::FlowNode *> get_all_nodes(void) const;
    void dump_dot(DominatorTree &DT, PostDominatorTree &PDT) const;

   protected:
    friend struct FlowNode;
    FlowNode &get_node(llvm::Value *);
    FlowNode &get_node_including_sinks(llvm::Value *);
    int next_id = 0;

   private:
    llvm::Function &m_func;
    std::map<llvm::Value *, std::unique_ptr<FlowNode>> m_nodes;
    std::map<llvm::Value *, std::unique_ptr<FlowNode>> m_sinks;
  };

}  // namespace alaska
