#include <alaska/OptimisticTypes.h>
#include <alaska/Utils.h>
#include <alaska/TypedPointer.h>
#include "llvm/ADT/TypeSwitch.h"
#include "llvm/AsmParser/Parser.h"

#include <alaska/Types.h>

// #define DEBUGLN(...) alaska::println(__VA_ARGS__)
#define DEBUGLN(...)

static void get_deep_struct_element_types_r(alaska::Type *t, std::vector<alaska::Type *> &out) {
  if (auto st = dyn_cast<alaska::StructType>(t)) {
    for (auto field : st->getFields()) {
      get_deep_struct_element_types_r(field, out);
    }
  } else {
    out.push_back(t);
  }
}

static std::vector<alaska::Type *> get_deep_struct_element_types(alaska::StructType *st) {
  std::vector<alaska::Type *> out;
  get_deep_struct_element_types_r(st, out);
  return out;
}



static bool type_lifts_into(alaska::Type *tolift, alaska::Type *into) {
  if (tolift == into) {
    DEBUGLN("lift equal: ", *tolift, " | ", *into);
    return true;
  }
  DEBUGLN("lift check: ", *tolift, " | ", *into);
  if (auto tlTP = dyn_cast<alaska::PointerType>(tolift)) {
    if (auto intoTP = dyn_cast<alaska::PointerType>(into)) {
      return type_lifts_into(tlTP->getElementType(), intoTP->getElementType());
    }
  }

  // if `tolift` is a vector type, and `into` is a struct, we need to check if the
  // first `n` fields of the struct are that type. If not, we cannot lift.
  // if (auto vec = dyn_cast_or_null<alaska::VectorType>(tolift)) {
  //   if (auto intoStruct = dyn_cast_or_null<StructType>(into)) {
  //     DEBUGLN("can ", *vec, " lift into ", *into);
  //     // Extrac the deep struct types from this structure
  //     auto types = get_deep_struct_element_types(intoStruct);
  //     auto elcount = vec->getElementCount();
  //     // If the vector could be referencing the first N elements of the struct,
  //     // it can be lifted into the struct type. Otherwise strict aliasing was violated.
  //     if (elcount.isVector() && elcount.getFixedValue() <= types.size()) {
  //       for (unsigned int i = 0; i < elcount.getFixedValue(); i++) {
  //         if (types[i] != vec->getElementType()) return false;
  //       }
  //       return true;
  //     }
  //   }
  //   return false;  // No dice.
  // }


  if (auto *pt = dyn_cast<alaska::PointerType>(tolift)) {
    DEBUGLN("tolift is a pointer");
    if (isa<alaska::PointerType>(into) || isa<alaska::PointerType>(into)) {
      DEBUGLN("into is also a pointer");
      return true;
    }
    // return false;
  }

  if (auto *st = dyn_cast<alaska::StructType>(into)) {
    DEBUGLN("into is a struct");
    if (st->getFieldCount() == 0) {
      return false;
    }
    auto firstType = st->getContainedType(0);
    return type_lifts_into(tolift, firstType);
  }
  return false;
}




void alaska::OptimisticTypes::analyze(llvm::Function &F) {
  ingest_function(F);
  reach_fixed_point();
}

void alaska::OptimisticTypes::analyze(llvm::Module &M) {
  for (auto &F : M) {
    ingest_function(F);
  }
  reach_fixed_point();
}


void alaska::OptimisticTypes::ingest_function(llvm::Function &F) {
  auto module = F.getParent();
  if (this->module)
    ALASKA_SANITY(this->module == module, "Cannot mix modules in an optimistictypes analysis");
  this->module = module;

  // TODO: what if this function affects the types in another function? (ret type)
  this->visit(F);

  // Now we have some initial state in `this->types`. We need to go through and
  // set all other pointers in the type and set them to underdefined.
  auto setUnderDefined = [&](llvm::Value *v) {
    auto t = v->getType();
    if (!t->isPointerTy()) return;
    auto found = m_types.find(v);

    if (found == m_types.end()) {
      m_types[v] = OTLatticePoint(PointType::Underdefined);
    }
  };

  for (auto &arg : F.args()) {
    setUnderDefined(&arg);
  }

  for (auto &BB : F) {
    for (auto &I : BB) {
      setUnderDefined(&I);
    }
  }
}

void alaska::OptimisticTypes::reach_fixed_point(void) {
  bool changed;
  do {
    changed = false;

    DEBUGLN("==============================================================");

    for (auto &[p, lp] : m_types) {
      // If p is a call instruction (it must be a pointer), meet it with all the return instruction
      // values in the function it is calling.

      if (auto call = dyn_cast<llvm::CallBase>(p)) {
        if (call->getType()->isPointerTy()) {        // If the call is a pointer...
          if (auto f = call->getCalledFunction()) {  // And the call is direct...
            if (not f->empty()) {                    // And the function has a body...
              for (auto &BB : *f) {
                if (auto retInst = dyn_cast_or_null<llvm::ReturnInst>(BB.getTerminator())) {
                  auto t = get_lattice_point(retInst->getReturnValue());
                  if (t.is_defined()) {
                    // errs() << "meet " << *p << " with " << *retInst << "\n";
                    changed |= lp.meet(t);
                  }
                }
              }
            }
          }
        }
      }


      for (auto &use : p->uses()) {
        // TODO: fixme :)
        std::vector<OTLatticePoint::Base *> incoming;
        OTLatticePoint temp;
        auto *user = use.getUser();

        if (auto load = dyn_cast<llvm::LoadInst>(user)) {
          // if the user is a load...
          // and the pointer operand is p...
          // and the type of the load is a pointer...
          // then the result of the load needs to be
          if (load->getPointerOperand() == p && load->getType()->isPointerTy()) {
            auto t = get_lattice_point(load);
            DEBUGLN("\e[31mload ", t, " lp=", lp, "\e[0m");
            DEBUGLN("p = ", *p);
            DEBUGLN("inst = ", *load);
            if (t.is_defined()) {
              // There is a little extra work we have to do here, unfortunately.
              // Because the clang developers are lazy people, they often omit a
              // GEP if you are loading from the first field of a structure.
              // Therefore, if we see that the result of this load is some T,
              // the source pointer must be a T*. However, If lp is a typed
              // pointer to a struct, where the first field is a T, then we
              // should *not* meet it with a T*. This nonsense is only needed
              // because if a load is a phi with itself and another pointer,
              // it will always become ⊤, as there is no GEP to assign the type
              // T* to.
              bool do_meet = true;
              if (lp.is_defined()) {
                if (type_lifts_into(lp.get_state(), t.get_state())) {
                  do_meet = false;
                } else if (auto tp = dyn_cast_or_null<alaska::PointerType>(lp.get_state())) {
                  if (auto st = dyn_cast<alaska::StructType>(tp->getElementType())) {
                    auto types = get_deep_struct_element_types(st);
                    // if the first type can lift into t's state, don't meet.
                    if (type_lifts_into(types[0], t.get_state())) {
                      do_meet = false;
                    }
                  }
                }
              }
              if (do_meet) {
                temp.set_state(alaska::PointerType::get(t.get_state()));
                changed |= lp.meet(temp);
              }
            }
          }
        }

        // If `p` is used as an incoming edge on a phi, add the phi's lp to incoming
        if (auto phi = dyn_cast<llvm::PHINode>(user)) {
          changed |= lp.meet(get_lattice_point(phi));
        }


        if (auto freeze = dyn_cast<llvm::FreezeInst>(user)) {
          changed |= lp.meet(get_lattice_point(freeze));
        }

        if (auto call = dyn_cast<llvm::CallBase>(user)) {
          auto arg_index = use.getOperandNo();
          Function *f = call->getCalledFunction();
          if (f) {
            // First, meet all the passed-in arguments according to the lattice
            // points for the function's arguments
            DEBUGLN(*p, " used in call ", *call, " at position ", arg_index);
            if (arg_index <= f->getNumOperands()) {
              auto *arg = f->getArg(arg_index);
              if (arg) {
                auto t = get_lattice_point(arg);
                if (t.is_defined()) {
                  changed |= lp.meet(t);
                }
              }
            }
          }
        }
      }


      if (auto phi = dyn_cast<PHINode>(p)) {
        // If the value `p` is a phi node, then it's type must unify with the incoming edges' types.
        for (auto &in : phi->incoming_values()) {
          auto v = in.get();
          changed |= lp.meet(get_lattice_point(v));
        }
      }
    }

  } while (changed);
}



void alaska::OptimisticTypes::use(llvm::Value *v, alaska::Type *t) {
  DEBUGLN("use ", *v, " as ", *t);
  auto found = m_types.find(v);

  auto lp = OTLatticePoint(PointType::Defined, t);
  if (found == m_types.end()) {
    m_types[v] = lp;
  } else {
    m_types[v].meet(lp);
  }
}



alaska::OTLatticePoint alaska::OptimisticTypes::get_lattice_point(llvm::Value *v) {
  if (auto cv = dyn_cast_or_null<llvm::Constant>(v)) {
    if (cv->isNullValue()) {
      return OTLatticePoint(PointType::Underdefined);
    }
  }

  return m_types[v];
}


void alaska::OptimisticTypes::dump(void) {
  int num_def = 0;
  int num_udef = 0;
  int num_odef = 0;
  std::set<llvm::Function *> funcs;

  for (auto &[p, lp] : m_types) {
    if (auto inst = dyn_cast<llvm::Instruction>(p)) funcs.insert(inst->getFunction());
    if (auto arg = dyn_cast<llvm::Argument>(p)) funcs.insert(arg->getParent());
  }


  auto dump_value = [&](llvm::Value *val) {
    if (val->getType()->isPointerTy()) {
      auto lp = get_lattice_point(val);

      if (lp.is_defined()) {
        llvm::errs() << "\e[32m";
        num_def++;
      }
      if (lp.is_overdefined()) {
        llvm::errs() << "\e[93m";
        num_odef++;
      }
      if (lp.is_underdefined()) {
        llvm::errs() << "\e[91m";
        num_udef++;
      }
      llvm::errs() << *val << "\e[0m :: \e[34m" << lp << "\e[0m";

      // if (lp.is_defined()) {
      //   if (auto t = ctx.convert(lp.get_state())) {
      //     llvm::errs() << "  (" << *t << ")";
      //   }
      // }
      // if (auto t = alaska::extractTypeMD(val)) {
      //   llvm::errs() << "  (" << *t << ")";
      // }
    } else {
      llvm::errs() << "\e[90m";
      errs() << *val;
      llvm::errs() << "\e[0m";
    }
  };



  for (auto f : funcs) {
    if (f->empty()) continue;
    errs() << *f->getReturnType() << " " << f->getName() << "(\n";
    for (auto &arg : f->args()) {
      errs() << "   ";
      dump_value(&arg);
      errs() << "\n";
    }
    if (f->isVarArg()) {
      errs() << "   ...\n";
    }

    errs() << ") {\n";

    for (auto &BB : *f) {
      BB.printAsOperand(errs(), false);
      errs() << ":\n";
      for (auto &I : BB) {
        dump_value(&I);
        errs() << "\n";
      }
      errs() << "\n";
    }

    errs() << "}\n\n";
  }

  // return;

  for (auto &[p, lp] : m_types) {
    if (lp.is_defined()) {
      num_def++;
    }
    if (lp.is_overdefined()) {
      num_odef++;
    }
    if (lp.is_underdefined()) {
      num_udef++;
    }
  }

  printf("def: %d, odef: %d, udef: %d\n", num_def, num_odef, num_udef);
}




llvm::MDNode *alaska::OptimisticTypes::embedType(alaska::Type *type) {
  auto it = m_mdMap.find(type);
  if (it != m_mdMap.end()) {
    return it->second;
  }

  auto &ctx = this->module->getContext();

  auto createString = [&](StringRef str) {
    return MDString::get(ctx, str);
  };


  auto m = llvm::TypeSwitch<alaska::Type *, llvm::MDNode *>(type)
               .Case<alaska::PointerType>([&](auto tp) {
                 return MDNode::get(ctx, {createString("ptr"), embedType(tp->getElementType())});
               })
               .Case<alaska::StructType>([&](auto st) {
                 // if (not st->hasName()) {
                 //   st->setName("anon");
                 // }
                 return MDNode::get(ctx, {createString("struct"), createString(st->getName())});
               })
               .Case<alaska::PrimitiveType>([&](alaska::PrimitiveType *pt) {

                 std::string raw;
                 llvm::raw_string_ostream raw_writer(raw);
                 pt->getWrapped()->print(raw_writer, false, true);
                 return MDNode::get(ctx, {createString("prim"), createString(raw)});
               })
               .Default([&](alaska::Type *t) {
                 std::string raw;
                 llvm::raw_string_ostream raw_writer(raw);
                 // t->print(raw_writer, false, true);
                 return MDNode::get(ctx, {createString("other"), createString(raw)});
               });


  this->m_mdMap[type] = m;
  return m;
}

void alaska::OptimisticTypes::embed(void) {
  // Dont do anything if we don't have a module yet.
  if (this->module == nullptr) return;



  for (auto &[p, lp] : m_types) {
    if (not lp.is_defined()) continue;

    auto type = lp.get_state();

    auto m = embedType(type);
    if (m == nullptr) continue;

    if (auto inst = dyn_cast<Instruction>(p)) {
      inst->setMetadata("alaska.type", m);
    }
  }


  for (auto &F : *this->module) {
    if (F.empty()) continue;

    std::vector<llvm::Metadata *> argsVec;
    for (auto &arg : F.args()) {
      alaska::Type *t = alaska::convertType(arg.getType());
      if (auto lp = get_lattice_point(&arg); lp.is_defined()) {
        t = lp.get_state();
      }

      argsVec.push_back(embedType(t));
    }
    auto m = MDTuple::get(F.getContext(), argsVec);
    F.setMetadata("alaska.type", m);
  }


  auto all_types = this->module->getOrInsertNamedMetadata("alaska.alltypes");

  std::set<llvm::MDNode *> nodes;
  for (auto [_, md] : m_mdMap)
    nodes.insert(md);
  for (auto node : nodes)
    all_types->addOperand(node);
}




void alaska::OptimisticTypes::visitGetElementPtrInst(llvm::GetElementPtrInst &I) {
  // The pointer operand is used as the type listed in the instruction.
  use(I.getPointerOperand(), alaska::PointerType::get(alaska::convertType(I.getSourceElementType())));
  if (I.hasAllConstantIndices()) {
    std::vector<llvm::Value *> inds(I.idx_begin(), I.idx_end());
    auto t = llvm::GetElementPtrInst::getIndexedType(I.getSourceElementType(), inds);
    if (t) use(&I, alaska::PointerType::get(alaska::convertType(t)));
  }
}

void alaska::OptimisticTypes::visitLoadInst(llvm::LoadInst &I) {
  // The pointer operand must be a pointer to the type of the result of the load.
  use(I.getPointerOperand(), alaska::PointerType::get(alaska::convertType(I.getType())));
}

void alaska::OptimisticTypes::visitStoreInst(llvm::StoreInst &I) {
  auto stored = I.getValueOperand();
  // The pointer operand must be a pointer to the type of the value stored to memory
  use(I.getPointerOperand(), alaska::PointerType::get(alaska::convertType(stored->getType())));
}

void alaska::OptimisticTypes::visitAllocaInst(llvm::AllocaInst &I) {
  use(&I, alaska::PointerType::get(alaska::convertType(I.getAllocatedType())));
}


bool alaska::OTLatticePoint::meet(const OTLatticePoint::Base &in) {
  DEBUGLN("Meet: ", *this, " and ", in);
  // Can't do anything if this point is already overdefined
  if (this->is_overdefined()) {
    DEBUGLN("already overdefined.\n");
    return false;
  }


  // If the incoming value is overdefined, we are also overdefined
  if (in.is_overdefined()) {
    DEBUGLN("in is overdefined. become overdefined\n");
    set_overdefined();
    return true;  // We changed.
  }

  // If the incoming is underdefined, we don't update
  if (in.is_underdefined()) {
    DEBUGLN("in is underdefined. no change\n");
    // no change. The incoming point can provide us with no information
    return false;
  }

  // If we are underdefined and in is defined, then just meet that state.
  if (this->is_underdefined()) {
    DEBUGLN("We are underdefined, become in\n");
    set_state(in.get_state());
    return true;
  }

  ALASKA_SANITY(this->get_state() != NULL, "This state is null");
  ALASKA_SANITY(in.get_state() != NULL, "Incoming state is null");

  // No change if 'in' is equal to us
  if (in.get_state() == this->get_state()) {
    DEBUGLN("equal. Do nothing.\n");
    return false;
  }


  // If our state can be lifted into their state...
  DEBUGLN("checking case A");
  if (type_lifts_into(this->get_state(), in.get_state())) {
    // ... become their state
    set_state(in.get_state());
    return true;
  }

  // DEBUGLN("checking case B");
  if (type_lifts_into(in.get_state(), this->get_state())) {
    DEBUGLN("reverse meets. Do nothing\n");
    // if the incoming state can lift into ours, we should not become overdefined
    return false;
  }
  DEBUGLN("Become overdefined: cannot lift\n");
  set_overdefined();
  return true;
}

bool alaska::OTLatticePoint::join(const OTLatticePoint::Base &incoming) {
  // Nothing.
  return false;
}




static alaska::Type *parseTypeMD(llvm::Module &M, llvm::Metadata *m) {
  if (auto md = dyn_cast<llvm::MDNode>(m)) {
    auto &ctx = md->getContext();
    if (md->getNumOperands() == 1) {
      // Handle only "ptr"
      if (MDString *tag = dyn_cast<MDString>(md->getOperand(0))) {
        if (tag->getString() == "ptr") {
          return alaska::convertType(llvm::PointerType::get(ctx, 0));
        }
      }
    }

    if (md->getNumOperands() == 2) {
      if (MDString *tag = dyn_cast<MDString>(md->getOperand(0))) {
        // Pointer to a type
        if (tag->getString() == "ptr") {
          return alaska::PointerType::get(parseTypeMD(M, md->getOperand(1).get()));
        }

        // {"struct", "struct.X"}
        if (tag->getString() == "struct") {
          MDString *tag2 = dyn_cast<MDString>(md->getOperand(1));
          ALASKA_SANITY(tag2 != nullptr, "The second tag in a struct metadata must be a string.");
          return alaska::convertType(llvm::StructType::getTypeByName(ctx, tag2->getString()));
        }

        // {"other", "..."} (other llvm type)
        if (tag->getString() == "other") {
          MDString *tag2 = dyn_cast<MDString>(md->getOperand(1));
          ALASKA_SANITY(tag2 != nullptr, "The second tag in a struct metadata must be a string.");
          llvm::SMDiagnostic Err;
          return alaska::convertType(llvm::parseType(tag2->getString(), Err, M, nullptr));
        }
      }
    }
  }
  return nullptr;
}

alaska::Type *alaska::extractTypeMD(llvm::Value *v) {
  if (auto *inst = dyn_cast<Instruction>(v)) {
    if (not inst->hasMetadata("alaska.type")) return nullptr;

    return parseTypeMD(*inst->getFunction()->getParent(), inst->getMetadata("alaska.type"));
  }

  if (auto *arg = dyn_cast<llvm::Argument>(v)) {
    auto func = arg->getParent();

    if (auto *mdtup = dyn_cast_or_null<llvm::MDTuple>(func->getMetadata("alaska.type"))) {
      if (mdtup->getNumOperands() <= arg->getArgNo()) return nullptr;
      return parseTypeMD(*func->getParent(), mdtup->getOperand(arg->getArgNo()));
    }
  }


  return nullptr;
}
