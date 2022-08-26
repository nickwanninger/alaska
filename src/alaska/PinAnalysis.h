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
#include <memory> // shared_ptr
#include <set>

using namespace llvm;

namespace alaska {

class PinAnalysis;

enum NodeType {
  Conjure,   // CallInst, IntToPtrInst, Argument, etc..
  Transform, // GetElementPtrInst, BitCastInst, etc..
  Access,    // Pointer operand of StoreInst, LoadInst
};

/**
 * Implementation of the nodes in a PinTrace tree
 *
 * Trace nodes exist to map sibling operations between their handle and pin
 * variant. They track parent nodes (of which there can be several) as well as
 * their child nodes. If a node is of the `Conjure` type, it represents a pointer
 * entering the control flow, and the following transformation can be done:
 *
 *     %handle = ??
 *     %pin = alaska_translate(%handle)
 *
 * If the handle is the result of a GEP instruction, for example, it has the type
 * `Transform`, as it transforms an existing pointer into a new pointer. In this case,
 * it is valid to trasform the parent node in the same way:
 *
 *     %handle.2 = getelementptr %handle.1, ...
 *     %pin.2    = getelementptr %pin.1,    ...
 * or:
 *     %handle.2 = bitcast %handle.1, ...
 *     %pin.2    = bitcast %pin.1,    ...
 *
 * The final NodeType is `Access`. These nodes act as sinks for all parent
 * nodes, and are where the program actually accesses memory. These nodes are
 * special as their %handle value is actually the instruction which accesses
 * the handle and not the handle itself. The value being accessed will be the
 * parent node. The transformation will often look like the following:
 *
 *    %val = load %parent.handle    becomes...    %val = load %parent.pin
 *    store %parent.handle, %val    becomes...    store %parent.pin, %val
 *
 * Due to the parent/child relationship, there exists a flow between nodes in
 * this tree, where one node depends on the value of another, and can reproduce
 * the behavior of the original in the pin address space using the %pin of it's
 * parent node. This flow may look like the following:
 *   +-----------+    +-----------+    +--------+
 *   |  Conjure  ├-┬->| Transform ├-┬->| Access |
 *   +-----------+ |  +-----------+ |  +--------+
 *                 |  +--------+    |  +--------+
 *                 +->| Access |    +->| Access |
 *                    +--------+       +--------+
 * In this example, a pointer is conjured, either by calling a `malloc` type
 * function or recieving it as an argument. The program then transforms it
 * by some mechanism -- for example, by GEP-ing a struct field -- and accesses
 * it by loads or stores.
 */
class PinNode {
public:
  NodeType type(void) const {
    return m_type;
  }

  llvm::Value *handle(void) {
    return m_handle;
  }

  bool unpinned(void) const {
    return m_pin == nullptr;
  }

  // Return the sibling pin that
  llvm::Value *pin(void) {
    return m_pin;
  }

  void set_pin(llvm::Value *val) {
    // if (val->getValueID())
    auto &ctx = val->getContext();
    if (val->getType() != llvm::Type::getVoidTy(ctx)) {
      val->setName("pin." + std::to_string(m_id));
    }
    m_pin = val;
  }

  // Accessor methods into the parent
  PinNode *parent(void) {
    return m_parent;
  }
  llvm::Value *parent_pin(void) {
    if (m_parent == NULL) {
      // abort();
    }
    return m_parent == NULL ? NULL : m_parent->pin();
  }
  llvm::Value *parent_handle(void) {
    return m_parent == NULL ? NULL : m_parent->handle();
  }

  const auto &children(void) const {
    return m_children;
  }

  // determine if you need this node or not (are any subnodes important or are they accesses)
  bool important(void) const;

  // If there isn't a pin for this instruction, codegen one.
  llvm::Value *codegen_pin(void);

protected:
  friend alaska::PinAnalysis;
  PinNode(int id, PinAnalysis &trace, NodeType type, PinNode *parent, llvm::Value *handle)
      : m_id(id), m_trace(trace), m_type(type), m_parent(parent), m_handle(handle) {

    auto &ctx = m_handle->getContext();
    if (m_handle->getType() != llvm::Type::getVoidTy(ctx)) {
      m_handle->setName("handle." + std::to_string(m_id));
    }
    if (parent)
      parent->m_children.insert(this);
  }

private:
  int m_id;
  PinAnalysis &m_trace;
  NodeType m_type;
  PinNode *m_parent = NULL;
  llvm::Value *m_handle = nullptr;
  llvm::Value *m_pin = nullptr;
  std::set<PinNode *> m_children;
};

// There is a PinTrace per LLVM Function. It manages
class PinAnalysis {
public:
  PinAnalysis(llvm::Function &func) : m_func(func) {}

  llvm::Function &func(void) {
    return m_func;
  }

  // Get an existing node, returning null if there isn't already one
  PinNode *get_node(llvm::Value *val) const;
  PinNode *add_node(llvm::Value *val);
  PinNode *get_or_add_node(llvm::Value *val);
  PinNode *add_root(llvm::Value *val);

  void compute_trace(void);
  // Inject calls top `alaska_pin` as needed
  void inject_pins(void);

private:
  int next_id = 0;
  // Mappings from Values (often instructions) to their trace nodes
  std::map<llvm::Value *, std::unique_ptr<PinNode>> m_nodes;
  // The roots of a trace
  std::set<PinNode *> m_roots;

  llvm::Function &m_func;
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
} // namespace alaska
