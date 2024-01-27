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


#include <alaska/TypedPointer.h>
#include "llvm/IR/LLVMContext.h"
#include "llvm/ADT/DenseMap.h"

///////////////////////////////////////////////////////////////////////////////
alaska::TypedPointer *alaska::TypedPointer::get(Type *EltTy) {
  static llvm::DenseMap<std::pair<llvm::LLVMContext *, llvm::Type *>, alaska::TypedPointer *>
      cachedTypedPointers;

  assert(EltTy && "Can't get a pointer to <null> type!");
  assert(isValidElementType(EltTy) && "Invalid type for pointer element!");
  auto *&Entry = cachedTypedPointers[std::make_pair(&EltTy->getContext(), EltTy)];

  if (!Entry) Entry = new alaska::TypedPointer(EltTy);
  return Entry;
}

alaska::TypedPointer::TypedPointer(Type *E, unsigned AddrSpace)
    : Type(E->getContext(), TypedPointerTyID)
    , PointeeTy(E) {
  ContainedTys = &PointeeTy;
  NumContainedTys = 1;
  setSubclassData(AddrSpace);
}

bool alaska::TypedPointer::isValidElementType(Type *ElemTy) {
  return !ElemTy->isVoidTy() && !ElemTy->isLabelTy() && !ElemTy->isMetadataTy() &&
         !ElemTy->isTokenTy() && !ElemTy->isX86_AMXTy();
}
