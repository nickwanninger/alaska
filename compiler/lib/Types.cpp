#include <alaska/Types.h>

#include "llvm/ADT/TypeSwitch.h"



static alaska::TypeContext typeContext;

alaska::Type *alaska::convertType(llvm::Type *t) {
  return typeContext.convert(t);
}


alaska::PointerType *alaska::PointerType::get(alaska::Type *elTy) {
  return elTy->getContext().getPointerTo(elTy);
}




llvm::raw_ostream &alaska::operator<<(llvm::raw_ostream &os, alaska::Type &t) {
  llvm::TypeSwitch<alaska::Type *>(&t)
      .Case<alaska::StructType>([&](alaska::StructType *t) {
        os << "(struct " << t->getName();
        for (auto *field : t->getFields()) {
          os << " " << *field;
        }
        os << ")";
      })
      .Case<alaska::PointerType>([&](auto *t) {
        os << "(ptr " << *t->getElementType() << ")";
      })
      .Case<alaska::ArrayType>([&](alaska::ArrayType *t) {
        os << "(arr " << *t->getElementType() << " " << t->getNumElements() << ")";
      })
      .Case<alaska::PrimitiveType>([&](auto *t) {
        os << ":" << *t->getWrapped();
      })
      .Case<alaska::VarType>([&](auto *t) {
        os << "t" << t->getID();
      })
      .Default([&](auto *t) {
        os << "Other " << t;
      });
  return os;
}




template <typename IndT>
static alaska::Type *getTypeAtIndex(alaska::Type *ty, IndT idx) {
  if (auto agg = dyn_cast<alaska::StructType>(ty)) {
    return agg->getTypeAtIndex(idx);
  }

  if (auto agg = dyn_cast<alaska::ArrayType>(ty)) {
    return agg->getElementType();
  }
  return nullptr;
}

alaska::Type *alaska::getIndexedType(alaska::Type *Agg, ArrayRef<unsigned> Idxs) {
  if (Idxs.empty()) return Agg;
  for (auto V : Idxs.slice(1)) {
    Agg = getTypeAtIndex(Agg, V);
    if (!Agg) break;
  }
  return Agg;
}

alaska::Type *alaska::getIndexedType(alaska::Type *Agg, ArrayRef<llvm::Value *> Idxs) {
  if (Idxs.empty()) return Agg;
  for (auto V : Idxs.slice(1)) {
    Agg = getTypeAtIndex(Agg, V);
    if (!Agg) break;
  }
  return Agg;
}
