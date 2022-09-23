#include "./PinAnalysis.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstVisitor.h"

// Compute parent and type for instructions before inserting them into the trace.
class TraceVisitor : public llvm::InstVisitor<TraceVisitor> {
 public:
  // which value does this depend on (What handle needs to be translated for
  // me to use it). If this is NULL, the node is a root and must create a call
  // to `alaska_pin` before any child can use it
  llvm::Value *parent = NULL;
  // The type of the usage. Default to conservative case
  alaska::NodeType type = alaska::Conjure;

  // #if 0
  void visitGetElementPtrInst(llvm::GetElementPtrInst &I) {
    type = alaska::Transform;
    parent = I.getPointerOperand();
  }

  void visitStoreInst(llvm::StoreInst &I) {
    type = alaska::Access;
    parent = I.getPointerOperand();
  }

  void visitLoadInst(llvm::LoadInst &I) {
    type = alaska::Access;
    parent = I.getPointerOperand();
  }

  void visitCastInst(llvm::CastInst &I) {
    type = alaska::Transform;
    parent = I.getOperand(0);
  }

  void visitIntToPtrInst(llvm::IntToPtrInst &I) {
    type = alaska::Conjure;
    parent = NULL;
  }
  // #endif

  void visitInstruction(llvm::Instruction &I) {
    type = alaska::Conjure;
    parent = NULL;
  }
};

alaska::PinNode *alaska::PinAnalysis::get_node(llvm::Value *val) const {
  auto it = m_nodes.find(val);
  if (it == m_nodes.end()) return NULL;
  return it->second.get();
}

alaska::PinNode *alaska::PinAnalysis::add_root(llvm::Value *val) {
  if (auto previouslyAdded = get_node(val)) {
    return previouslyAdded;
  }
  auto *r = new alaska::PinNode(next_id++, *this, alaska::Conjure, nullptr, val);
  m_nodes[val] = std::unique_ptr<alaska::PinNode>(r);
  m_roots.insert(r);

  // alaska::println("root: ", *val);
  for (auto user : val->users()) {
    // alaska::println("user: ", *user);
    add_node(user);
  }
  return r;
}

alaska::PinNode *alaska::PinAnalysis::add_node(llvm::Value *val) {
  if (auto previouslyAdded = get_node(val)) {
    return previouslyAdded;
  }
  alaska::PinNode *parent = NULL;
  alaska::PinNode *node = NULL;
  TraceVisitor v;
  llvm::Instruction *I = dyn_cast<llvm::Instruction>(val);
  if (I) v.visit(I);

  if (v.parent) parent = get_or_add_node(v.parent);

  node = new alaska::PinNode(next_id++, *this, v.type, parent, val);
  m_nodes[val] = std::unique_ptr<alaska::PinNode>(node);
  if (parent == NULL) {
    // m_roots.insert(node);
  }

  for (auto user : val->users()) {
    add_node(user);
  }
  return node;
}

alaska::PinNode *alaska::PinAnalysis::get_or_add_node(llvm::Value *val) {
  auto n = get_node(val);
  if (n) return n;
  return add_node(val);
}

static void dump_children(alaska::PinNode *node, int depth = 0) {
#ifdef ALASKA_DEBUG
  if (!node->important()) {
    // return; // llvm::errs() << "XXX ";
  }
  for (int i = 1; i < depth + 1; i++) {
    llvm::errs() << "  ";
  }
  switch (node->type()) {
    case alaska::Conjure:
      llvm::errs() << "Conjure ";
      break;
    case alaska::Transform:
      llvm::errs() << "Transform ";
      break;
    case alaska::Access:
      llvm::errs() << "Access ";
      break;
  }
  alaska::println("", *node->handle());
  auto children = node->children();
  for (auto *child : children) {
    dump_children(child, depth + 1);
  }
#endif
}

void alaska::PinAnalysis::compute_trace(void) {
  return;
  alaska::println("===========================");
  for (auto *root : m_roots) {
    if (root->important()) {
      dump_children(root);
      alaska::println();
    }
  }
}

void alaska::PinAnalysis::inject_pins(void) {
  for (auto *root : m_roots) {
    dump_children(root);
    root->codegen_pin();
  }

  // alaska::println(m_func);
}

bool alaska::PinNode::important(void) const {
  return true;
  // HACK: for some reason Alloca instructions appear here.
  if (auto a = dyn_cast<llvm::AllocaInst>(m_handle)) {
    // return false;
  }

  // return true;
  if (type() == alaska::Access) return true;
  for (auto *child : m_children) {
    if (child->type() == alaska::Access || child->important()) return true;
  }
  return false;
}

static llvm::Value *insert_translate_call_before(llvm::Instruction *inst, llvm::Value *val) {
  llvm::LLVMContext &ctx = inst->getContext();
  auto *M = inst->getParent()->getParent()->getParent();
  auto voidPtrType = llvm::Type::getInt8PtrTy(ctx, 0);
  auto translateFunctionType = llvm::FunctionType::get(voidPtrType, {voidPtrType}, false);
  auto translateFunction = M->getOrInsertFunction("alaska_pin", translateFunctionType).getCallee();

  llvm::IRBuilder<> b(inst);
  auto ptr = b.CreatePointerCast(val, voidPtrType);
  auto translatedVoidPtr = b.CreateCall(translateFunction, {ptr});
  auto translated = b.CreatePointerCast(translatedVoidPtr, val->getType());
  return translated;
}

static llvm::Value *insert_translate_call_after(llvm::Instruction *inst, llvm::Value *val) {
  return insert_translate_call_before(inst->getNextNode(), val);
}

// Compute parent and type for instructions before inserting them into the trace.
class PinCodegenVisitor : public llvm::InstVisitor<PinCodegenVisitor> {
 public:
  // the node to codegen for
  alaska::PinNode *node;
  // The value that was generated
  llvm::Value *generated = NULL;
  llvm::Instruction *I = NULL;

  // General interface to transform the result of an instruction using `alaska_pin`
  void handleNaiveProduction(llvm::Instruction *I, llvm::Instruction *insertLocation = nullptr) {
    if (!I->getType()->isPointerTy()) {
      // TODO: the analysis should catch this before reachiing this point i
      // think this stems from the fact I am not considering the fact that an
      // instruction (like load) can both consume and create a new pointer.
      // Instructions like call can actually consume several pointers, which is
      // even worse. May ned to rewrite the abstraction
      return;
    }
    if (insertLocation == nullptr) insertLocation = I;
    auto p = insert_translate_call_after(insertLocation, I);
    node->set_pin(p);

    // alaska::println("naive from: ", *I);
    // alaska::println("        to: ", *p);
  }

  void visitCallInst(llvm::CallInst &I) {
    // alaska::println("Call Instruction");
    handleNaiveProduction(&I);
  }

  void visitInvokeInst(llvm::InvokeInst &I) {
    // Invoke sucks. Its basically a branch instruction.
    auto landingBB = I.getLandingPadInst()->getParent();

    handleNaiveProduction(&I, landingBB->getFirstNonPHI());
  }

  void visitSelectInst(llvm::SelectInst &I) {
    // alaska::println("Select Instruction");
    handleNaiveProduction(&I);
  }

  void visitGetElementPtrInst(llvm::GetElementPtrInst &I) {
    // alaska::println("GEP Instruction");
    llvm::IRBuilder<> b(I.getNextNode());

    if (node->parent_pin() != NULL) {
      std::vector<llvm::Value *> inds;
      for (auto &ind : I.indices())
        inds.push_back(ind);
      node->set_pin(b.CreateGEP(node->parent_pin(), inds));
    } else {
      handleNaiveProduction(&I);
    }
  }

  void visitAllocaInst(llvm::AllocaInst &I) {
    node->set_pin(&I);
    // I.setOperand(1, node->parent_pin());
  }

  void visitStoreInst(llvm::StoreInst &I) {
    alaska::println("inst ", I);
    alaska::println("store from ", *I.getPointerOperand());
    I.setOperand(1, node->parent_pin());
    // alaska::println();
  }

  void visitLoadInst(llvm::LoadInst &I) {
    // Loads are interesting because they can Conjure a pointer, but can also
    // consume one as an Access. If a load has a parent node, it must load from
    // the pin of that parent. If it has children, it must generate a call to
    // `alaska_pin` to pin the newly acquired pointer
    //
    // 1. Handle the Access case
    if (node->parent()) I.setOperand(0, node->parent_pin());
    // 2. handle the Conjure case.
    if (node->children().size() > 0 && node->handle()->getType()->isPointerTy() /* wuh. */) handleNaiveProduction(&I);
  }

  void visitCastInst(llvm::CastInst &I) {
    // alaska::println("cast: ", I);
    if (node->parent_pin()) {
      llvm::IRBuilder<> b(I.getNextNode());
      node->set_pin(b.CreateCast(I.getOpcode(), node->parent_pin(), I.getType()));
    } else {
      handleNaiveProduction(&I);
    }
  }

  void visitPHINode(llvm::PHINode &I) {
    // Insert *after* the last phi node in the current basic block. We can
    // ensure there is at least one, as we are visiting it, so the
    // getPrevNode() will never be null here. The reason we need to insert
    // after the last phi and not after the first non-phi is to maintain
    // dominance in case the first non-phi consumes the pin we are creating.
    auto target = I.getParent()->getFirstNonPHI()->getPrevNode();
    handleNaiveProduction(&I, target);
  }

  void visitInstruction(llvm::Instruction &I) {
    alaska::println("Unhandled instruction! ", I);
    exit(-1);
  }
};

llvm::Value *alaska::PinNode::codegen_pin(void) {
  if (!important()) return nullptr;

  if (m_pin != nullptr) {
    alaska::println("WARN: codegen_pin was called multiple times for ", *handle());
    return m_pin;
  }

  /*
   * There are a few value types which need to be handled specially:
   *  - BitCast and co.
   *  - Argument
   *  - InlineGEP?
   *
   *  These all kinda fall into the "Operator" category in LLVM
   *  see: https://llvm.org/doxygen/classllvm_1_1Operator.html
   */

  m_pin = nullptr;

  // alaska::println("handle: ", *handle());
  PinCodegenVisitor v;
  v.node = this;

  if (auto I = dyn_cast<llvm::Instruction>(handle())) {
    v.visit(I);

    // if (parent_pin() != NULL) {
    //   v.visit(I);
    // } else {
    //   alaska::println("!dead!");
    //   v.handleNaiveProduction(I);
    // }
  } else {
    if (auto arg = dyn_cast<llvm::Argument>(handle())) {
      auto F = arg->getParent();
      auto &BB = F->front();
      auto target = BB.getFirstNonPHIOrDbg();

      set_pin(insert_translate_call_before(target, arg));
    } else if (auto bitcastOperator = dyn_cast<llvm::BitCastOperator>(handle())) {
      // alaska::println("BitCastOperator", *bitcastOperator);
      // We don't know how to handle this right now.
      assert(bitcastOperator->getNumUses() == 1);
      llvm::Value *user = *bitcastOperator->users().begin();
      // alaska::println("user:", *user);
      exit(-1);

    } else if (auto gepOperator = dyn_cast<llvm::GEPOperator>(handle())) {
      // alaska::println("GEPOperator", *gepOperator);
      exit(-1);
      // alaska::println("GEPOperator");
      // assert(gepOperator->getNumUses() == 1);
      // llvm::Value *user = *gepOperator->users().begin();
      // alaska::println("user:", *user);

      // for (auto user : gepOperator->users()) {
      //   alaska::println("  user:", *user);
      // }
    }
  }

  for (auto *c : m_children) {
    c->codegen_pin();
  }

  return m_pin;
}
