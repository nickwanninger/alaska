#include <alaska/OptimisticTypes.h>
#include <alaska/Utils.h>
#include <alaska/TypedPointer.h>

alaska::OptimisticTypes::OptimisticTypes(llvm::Function &F) { analyze(F); }


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

    for (auto &[p, lp] : m_types) {
      for (auto &use : p->uses()) {
        // TODO: fixme :)
        std::vector<OTLatticePoint::Base *> incoming;
        OTLatticePoint temp;
        auto *user = use.getUser();

        if (auto load = dyn_cast<llvm::LoadInst>(user)) {
          if (load->getPointerOperand() == p && load->getType()->isPointerTy()) {
            auto t = get_lattice_point(load);
            if (t.is_defined()) {
              temp.set_state(alaska::TypedPointer::get(t.get_state()));
              changed |= lp.meet(temp);
            }
          }
        }

        // If `p` is used as an incoming edge on a phi, add the phi's lp to incoming
        if (auto phi = dyn_cast<llvm::PHINode>(user)) {
          changed |= lp.meet(get_lattice_point(phi));
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



void alaska::OptimisticTypes::use(llvm::Value *v, llvm::Type *t) {
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
  for (auto &[p, lp] : m_types) {
    llvm::errs() << "value \e[32m" << *p << "\e[0m\n";
    llvm::errs() << " type \e[34m" << lp << "\e[0m\n";
  }
}

void alaska::OptimisticTypes::visitGetElementPtrInst(llvm::GetElementPtrInst &I) {
  use(I.getPointerOperand(), alaska::TypedPointer::get(I.getSourceElementType()));
}

void alaska::OptimisticTypes::visitLoadInst(llvm::LoadInst &I) {
  // The pointer operand must be a pointer to the type of the result of the load.
  use(I.getPointerOperand(), alaska::TypedPointer::get(I.getType()));
}

void alaska::OptimisticTypes::visitStoreInst(llvm::StoreInst &I) {
  auto stored = I.getValueOperand();
  // The pointer operand must be a pointer to the type of the value stored to memory
  use(I.getPointerOperand(), alaska::TypedPointer::get(stored->getType()));
}

void alaska::OptimisticTypes::visitAllocaInst(llvm::AllocaInst &I) {
  use(&I, alaska::TypedPointer::get(I.getAllocatedType()));
}


static bool type_lifts_into(llvm::Type *tolift, llvm::Type *into) {
  if (auto tlTP = dyn_cast<alaska::TypedPointer>(tolift)) {
    if (auto intoTP = dyn_cast<alaska::TypedPointer>(into)) {
      return type_lifts_into(tlTP->getElementType(), intoTP->getElementType());
    }
  }
  if (auto *pt = dyn_cast<llvm::PointerType>(tolift)) {
    if (isa<llvm::PointerType>(into) || isa<alaska::TypedPointer>(into)) {
      return true;
    }
    return false;
  }

  if (auto *st = dyn_cast<llvm::StructType>(into)) {
    if (st->getNumElements() == 0) {
      return false;
    }
    auto firstType = st->getElementType(0);

    if (firstType == tolift) {
      return true;
    }

    if (auto ost = dyn_cast<llvm::StructType>(firstType)) {
      return type_lifts_into(tolift, ost);
    }

    return false;
  }
  return false;
}


bool alaska::OTLatticePoint::meet(const OTLatticePoint::Base &in) {
  // Can't do anything if this point is already overdefined
  if (this->is_overdefined()) return false;


  // If the incoming value is overdefined, we are also overdefined
  if (in.is_overdefined()) {
    set_overdefined();
    return true;  // We changed.
  }

  // If the incoming is underdefined, we don't update
  if (in.is_underdefined()) {
    // no change. The incoming point can provide us with no information
    return false;
  }

  // If we are underdefined and in is defined, then just meet that state.
  if (this->is_underdefined()) {
    set_state(in.get_state());
    return true;
  }

  ALASKA_SANITY(this->get_state() != NULL, "This state is null");
  ALASKA_SANITY(in.get_state() != NULL, "Incoming state is null");

  // No change if 'in' is equal to us
  if (in.get_state() == this->get_state()) return false;

  // If our state can be lifted into their state...
  if (type_lifts_into(this->get_state(), in.get_state())) {
    // ... become their state
    set_state(in.get_state());
  } else {
    set_overdefined();
    return true;
  }

  return false;
}

bool alaska::OTLatticePoint::join(const OTLatticePoint::Base &incoming) {
  // Nothing.
  return false;
}
