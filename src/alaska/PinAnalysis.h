#pragma once


#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/DerivedUser.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Mangler.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/Analysis/AssumptionCache.h"


#include <map>
#include <unordered_map>
#include <memory>  // shared_ptr
#include <set>
#include <unordered_set>

using namespace llvm;

namespace alaska {

	class PinGraph;

  enum NodeType {
    Source,     // Malloc, Realloc, etc..
    Sink,       // Load, Store
    Transient,  // Gep, Phi, etc..
  };

  struct Node {
		int id;
    // The value at this node. Typically an Instruction, but Arguments also occupy a Node
		NodeType type;
		PinGraph &graph;
    llvm::Value *value;
		std::unordered_set<int> colors;

    Node(PinGraph &graph, llvm::Value *value);
		void add_in_edge(llvm::Use *);

		std::unordered_set<Node *> get_in_nodes(void) const;
		std::unordered_set<Node *> get_out_nodes(void) const;
   protected:

		friend class PinGraph;

    // edges to other nodes
    std::unordered_set<llvm::Use *> in;
    std::unordered_set<llvm::Use *> out;
		void populate_edges(void);
  };

  class PinGraph {
   public:
    PinGraph(llvm::Function &func);
    auto &func(void) { return m_func; }

		// get just the nodes that we care about (skip alloca and globals)
		std::unordered_set<alaska::Node *> get_nodes(void) const;
		// get all nodes, including those we don't really care about.
		std::unordered_set<alaska::Node *> get_all_nodes(void) const;

	protected:
		friend struct Node;
		Node &get_node(llvm::Value *);
		Node &get_node_including_sinks(llvm::Value *);
		int next_id = 0;
   private:
    llvm::Function &m_func;
		std::unordered_map<llvm::Value *, std::unique_ptr<Node>> m_nodes;
		std::unordered_map<llvm::Value *, std::unique_ptr<Node>> m_sinks;
  };

  inline void println() {
#ifdef ALASKA_DEBUG
    // base case
    llvm::errs() << "\n";
#endif
  }

  template <class T, class... Ts>
  inline void println(T const &first, Ts const &... rest) {
#ifdef ALASKA_DEBUG
    llvm::errs() << first;
    alaska::println(rest...);
#endif
  }
}  // namespace alaska
