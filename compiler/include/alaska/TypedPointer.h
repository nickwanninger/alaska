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
#pragma once
#include <llvm/IR/Type.h>


namespace alaska {
  class TypedPointer : public llvm::Type {
    explicit TypedPointer(llvm::Type *ElType, unsigned AddrSpace = 0);

    llvm::Type *PointeeTy;

   public:
    TypedPointer(const TypedPointer &) = delete;
    TypedPointer &operator=(const TypedPointer &) = delete;

    /// This constructs a pointer to an object of the specified type in a numbered
    /// address space.
    static TypedPointer *get(llvm::Type *ElementType);

    /// Return true if the specified type is valid as a element type.
    static bool isValidElementType(llvm::Type *ElemTy);

    /// Return the address space of the Pointer type.
    unsigned getAddressSpace() const { return getSubclassData(); }

    llvm::Type *getElementType() const { return PointeeTy; }

    /// Implement support type inquiry through isa, cast, and dyn_cast.
    static bool classof(const llvm::Type *T) { return T->getTypeID() == TypedPointerTyID; }
  };
}  // namespace alaska

