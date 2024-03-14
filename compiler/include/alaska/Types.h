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

#include <alaska/Utils.h>
#include <alaska/LatticePoint.h>

#include "llvm/IR/Type.h"
#include "llvm/IR/DerivedTypes.h"
#include <unordered_set>

namespace alaska {

  // Fwd decl
  class TypeContext;

  class Type {
   public:
    enum TypeID {
      VarTyID,        //< Type Variable.
      PrimitiveTyID,  //< Simple LLVM Primative Types
      StructTyID,     //< Structure Type
      PointerTyID,    //< Typed Pointer
      ArrayTyID,      //< Array Type
    };

   protected:
    friend class TypeContext;
    ALASKA_NOCOPY_NOMOVE(Type);
    Type(TypeContext &ctx, TypeID tid)
        : _ctx(ctx)
        , _typeid(tid) {}

   public:
    virtual ~Type() = default;

    // Get the internal TypeID from this instance.
    TypeID getTypeID(void) const { return _typeid; }
    // Get the context from this
    inline auto &getContext() { return _ctx; }
    friend llvm::raw_ostream &operator<<(llvm::raw_ostream &os, alaska::Type &t);



    unsigned getNumContainedTypes() const { return (unsigned)_containedTypes.size(); }
    Type *getContainedType(unsigned i) const {
      assert(i < getNumContainedTypes() && "Index out of range!");
      return _containedTypes[i];
    }
    const auto &getContainedTypes() const { return _containedTypes; }

   protected:
    void setContainedTypes(const std::vector<alaska::Type *> &tys) { _containedTypes = tys; }

   private:
    TypeContext &_ctx;
    const TypeID _typeid;
    std::vector<alaska::Type *> _containedTypes;
  };


  // A polymorphic variable type
  class VarType final : public Type {
   protected:
    friend class TypeContext;

    ALASKA_NOCOPY_NOMOVE(VarType);
    VarType(TypeContext &ctx, uint64_t id)
        : Type(ctx, Type::VarTyID)
        , _id(id) {}

   public:
    virtual ~VarType() = default;

    static bool classof(const Type *s) { return s->getTypeID() == Type::VarTyID; }
    inline uint64_t getID(void) { return _id; }

   private:
    uint64_t _id;
  };


  // A PrimativeType is a simple wrapper around an LLVM primative type.
  class PrimitiveType final : public Type {
   protected:
    friend class TypeContext;

    ALASKA_NOCOPY_NOMOVE(PrimitiveType);
    PrimitiveType(TypeContext &ctx, llvm::Type *t)
        : Type(ctx, Type::PrimitiveTyID)
        , _wrapped(t) {}

   public:
    virtual ~PrimitiveType() = default;

    static bool classof(const Type *s) { return s->getTypeID() == Type::PrimitiveTyID; }
    llvm::Type *getWrapped(void) { return _wrapped; }

   private:
    llvm::Type *_wrapped = nullptr;
  };


  class StructType final : public Type {
   protected:
    friend class TypeContext;

    ALASKA_NOCOPY_NOMOVE(StructType);
    StructType(TypeContext &ctx, llvm::StringRef name, const std::vector<alaska::Type *> &fields)
        : Type(ctx, Type::StructTyID)
        , _name(name.bytes_begin(), name.bytes_end()) {
      setContainedTypes(fields);
    }

   public:
    virtual ~StructType() = default;

    static bool classof(const Type *s) { return s->getTypeID() == Type::StructTyID; }

    inline auto getFieldCount(void) const { return getNumContainedTypes(); }
    inline const auto &getFields(void) const { return getContainedTypes(); }
    inline const std::string &getName(void) const { return _name; }

    inline bool validIndex(unsigned idx) const { return getFieldCount() > idx; }
    inline auto *getTypeAtIndex(unsigned idx) const { return getContainedType(idx); }
    alaska::Type *getTypeAtIndex(const Value *V) const {
      unsigned Idx = (unsigned)cast<Constant>(V)->getUniqueInteger().getZExtValue();
      ALASKA_SANITY(validIndex(Idx), "Invalid structure index!");
      return getTypeAtIndex(Idx);
    }

   private:
    std::string _name;
  };


  class PointerType final : public Type {
   protected:
    friend class TypeContext;
    ALASKA_NOCOPY_NOMOVE(PointerType);
    PointerType(TypeContext &ctx, alaska::Type *elTy)
        : Type(ctx, Type::PointerTyID)
        , _elementType(elTy) {}

   public:
    virtual ~PointerType() = default;


    static PointerType *get(alaska::Type *elTy);

    static bool classof(const Type *s) { return s->getTypeID() == Type::PointerTyID; }

    inline auto *getElementType(void) { return _elementType; }

   private:
    alaska::Type *_elementType;
  };


  class ArrayType final : public Type {
   protected:
    friend class TypeContext;
    ALASKA_NOCOPY_NOMOVE(ArrayType);
    ArrayType(TypeContext &ctx, alaska::Type *elTy, uint64_t numElements)
        : Type(ctx, Type::ArrayTyID)
        , _elementType(elTy)
        , _numElements(numElements) {}

   public:
    virtual ~ArrayType() = default;

    static bool classof(const Type *s) { return s->getTypeID() == Type::ArrayTyID; }

    inline auto *getElementType(void) { return _elementType; }
    inline auto getNumElements(void) { return _numElements; }

   private:
    alaska::Type *_elementType;
    uint64_t _numElements;
  };


  // A context in which the alaska types exist. This class encapuslates the conversion to alaska
  // types, as well as the assumptions which assign types to llvm values. At it's core, alaska
  // exists to map an LLVM type to a representation which is more friendly to the forms of analysis
  // we want to do. As such, each llvm type must be able to map into it's relative alaska type,
  // which has additional information we extract from optimistic analysis.
  class TypeContext final {
   public:
    TypeContext(llvm::Module &m)
        : _module(m) {}
    alaska::Type *convert(llvm::Type *);
    // Construct a fresh type variable
    alaska::VarType *freshVar(void);

    void runInference();
    void dump(llvm::Function &F);

    alaska::Type *getType(llvm::Value *);
    alaska::PointerType *getPointerTo(alaska::Type *elTy);

   private:
    void unify(alaska::Type *, alaska::Type *);

    std::set<std::pair<alaska::Type *, alaska::Type *>> _unifications;


    std::unordered_map<llvm::Type *, std::unique_ptr<alaska::Type>> _types;
    // All the polymorphic types in the program. These must be stored seperately, as
    // there exists one per llvm `ptr` in the llvm IR, and each must be unified together.
    std::unordered_map<uint64_t, std::unique_ptr<alaska::VarType>> _vars;
    // The assumptions in this program. This is effectively a map from value to
    // the type we think it is.
    std::unordered_map<llvm::Value *, alaska::Type *> _assump;

    // A mapping from element to the pointertype
    std::unordered_map<alaska::Type *, alaska::PointerType *> _pointers;

    llvm::Module &_module;
  };


  alaska::Type *getIndexedType(alaska::Type *Agg, ArrayRef<unsigned> Idxs);
  alaska::Type *getIndexedType(alaska::Type *Agg, ArrayRef<llvm::Value *> Idxs);


}  // namespace alaska
