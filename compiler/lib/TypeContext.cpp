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
#include <alaska/Types.h>
#include <alaska/TypedPointer.h>
#include "llvm/ADT/TypeSwitch.h"




void alaska::TypeContext::runInference(void) {

  auto dump_value = [&](const char *msg, llvm::Value *val) {
    auto t = getType(val);
    llvm::errs() << "  " << msg << ": \e[90m";
    llvm::errs() << *val << "\e[0m :: \e[34m" << *t << "\e[0m\n";
  };

  // Now, unify according to the semantics of each given value.
  for (auto [val, type] : _assump) {
    auto inst = dyn_cast_or_null<llvm::Instruction>(val);
    if (!inst) continue;

    if (auto phi = dyn_cast<PHINode>(inst)) {
      // All of the arguments to the phi must be the same type as the result of the phi
      dump_value("phi", phi);
      for (auto &incoming : phi->incoming_values()) {
        dump_value("inc", incoming.get());
        unify(type, getType(incoming.get()));
      }
      alaska::println();
    }

    if (auto load = dyn_cast<LoadInst>(inst)) {
      // A load, X, has some resultant type, T. This means that
      // the source operand *must* be of type T*.
      dump_value("load", load);
      dump_value("ptr", load->getPointerOperand());
      unify(getType(load->getPointerOperand()), getPointerTo(type));
      alaska::println();
    }

    if (auto store = dyn_cast<StoreInst>(inst)) {
      // A store of a value X :: T to some address Y means Y :: T*
      auto X = store->getValueOperand();
      auto T = getType(X);

      unify(getType(store->getPointerOperand()), getPointerTo(T));
      alaska::println();
    }

    if (auto gep = dyn_cast<GetElementPtrInst>(inst)) {
      // a gep of the following form:
      //   X = getelementptr T, Y, <inds>
      // Means that the following statements must be true
      //   X :: indexedType(T, <inds>)*
      //   Y :: T*
      dump_value("gep", gep);
      auto Y = gep->getPointerOperand();
      dump_value("operand", Y);
      auto T = convert(gep->getSourceElementType());
      unify(getType(Y), getPointerTo(T));
      std::vector<llvm::Value *> Idxs;

      for (auto &idx : gep->indices())
        Idxs.push_back(idx.get());
      auto fieldType = alaska::getIndexedType(T, Idxs);
      unify(type, getPointerTo(fieldType));
      alaska::println();
    }
  }

  alaska::println();
  alaska::println();
  alaska::println();
  alaska::println();
}


void alaska::TypeContext::unify(alaska::Type *ta, alaska::Type *tb) {
  // if (ta == tb) return;
  errs() << "\e[32munify " << *ta << " = " << *tb << "\e[0m\n";
  if (_unifications.contains(std::make_pair(tb, ta))) return;
  _unifications.insert(std::make_pair(ta, tb));
}


void alaska::TypeContext::dump(llvm::Function &F) {
  auto dump_value = [&](llvm::Value *val) {
    auto t = getType(val);

    if (val->getType()->isPointerTy()) {
      llvm::errs() << "\e[32m";
    } else {
      llvm::errs() << "\e[90m";
    }
    llvm::errs() << *val << "\e[0m :: \e[34m" << *t << "\e[0m";
  };


  errs() << *F.getReturnType() << " " << F.getName() << "(\n";
  for (auto &arg : F.args()) {
    errs() << "   ";
    dump_value(&arg);
    errs() << "\n";
  }
  if (F.isVarArg()) {
    errs() << "   ...\n";
  }

  errs() << ") {\n";

  for (auto &BB : F) {
    BB.printAsOperand(errs(), false);
    errs() << ":\n";
    for (auto &I : BB) {
      dump_value(&I);
      errs() << "\n";
    }
    errs() << "\n";
  }

  errs() << "}\n\n";


  errs() << "graph {\n";

  std::set<alaska::Type *> allUnifiedTypes;
  for (auto [a, b] : _unifications) {
    allUnifiedTypes.insert(a);
    allUnifiedTypes.insert(b);
  }

  for (auto t : allUnifiedTypes)
    errs() << "  t" << t << "[label=\"" << *t << "\",shape=box]\n";
  errs() << "\n";

  for (auto [a, b] : _unifications)
    errs() << "  t" << a << " -- t" << b << " // " << *a << " = " << *b << "\n";

  errs() << "}\n";
}



alaska::PointerType *alaska::TypeContext::getPointerTo(alaska::Type *elTy) {
  alaska::PointerType *&it = _pointers[elTy];
  if (it == nullptr) {
    it = new alaska::PointerType(*this, elTy);
  }
  return it;
}

alaska::Type *alaska::TypeContext::getType(llvm::Value *val) {
  // Check if the value has an assumption already.
  if (!_assump.contains(val)) {
    // If it doesn't, add one.
    auto t = convert(val->getType());
    _assump[val] = t;
    return t;
  }
  return _assump[val];
}


alaska::Type *alaska::TypeContext::convert(llvm::Type *type) {
  // `ptr` in LLVM must always return a new instance with a new type variable,
  // so don't even look into the type map.
  if (type->isPointerTy()) {
    return freshVar();
  }


  auto it = _types.find(type);
  if (it != _types.end()) {
    return it->second.get();
  }


  auto m = llvm::TypeSwitch<llvm::Type *, alaska::Type *>(type)
               .Case<llvm::StructType>([&](llvm::StructType *t) {
                 std::vector<alaska::Type *> fields;
                 for (auto field : t->elements()) {
                   fields.push_back(convert(field));
                 }
                 // TODO: fields!
                 return new alaska::StructType(*this, t->getName(), fields);
               })
               .Case<alaska::TypedPointer>([&](auto *t) {
                 return getPointerTo(convert(t->getElementType()));
               })
               // // TODO: array types should really be unique to their context (?)
               // .Case<llvm::ArrayType>([&](llvm::ArrayType *t) {
               //   return new alaska::ArrayType(convert(t->getElementType()), t->getNumElements());
               // })
               .Default([&](llvm::Type *t) {
                 return new alaska::PrimitiveType(*this, t);
               });

  this->_types[type] = std::unique_ptr<alaska::Type>(m);
  return m;
}

alaska::VarType *alaska::TypeContext::freshVar(void) {
  uint64_t id = _vars.size();
  auto t = new alaska::VarType(*this, id);
  _vars[id] = std::unique_ptr<alaska::VarType>(t);
  return t;
}

