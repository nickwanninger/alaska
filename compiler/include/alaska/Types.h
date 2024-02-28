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

  class Type {
   public:
    enum TypeID {
      VarTyID,        //< Type Variable.
      PrimitiveTyID,  //< Simple LLVM Primative Types
      StructTyID,     //< Structure Type
      PointerTyID,    //< Typed Pointer
      ArrayTyID,      //< Array Type
    };
    ALASKA_NOCOPY_NOMOVE(Type);
    Type(TypeID tid)
        : _typeid(tid) {}
    virtual ~Type() = default;
    TypeID getTypeID(void) const { return _typeid; }

    friend llvm::raw_ostream &operator<<(llvm::raw_ostream &os, alaska::Type &t);

   private:
    const TypeID _typeid;
  };


  // A polymorphic variable type
  class VarType final : public Type {
   public:
    ALASKA_NOCOPY_NOMOVE(VarType);
    VarType(uint64_t id)
        : Type(Type::VarTyID)
        , _id(id) {}
    virtual ~VarType() = default;

    static bool classof(const Type *s) { return s->getTypeID() == Type::VarTyID; }
    inline uint64_t getID(void) { return _id; }

   private:
    uint64_t _id;
    // The types which have unified with this type variable
    std::unordered_set<alaska::Type *> _unified;
  };


  // A PrimativeType is a simple wrapper around an LLVM primative type.
  class PrimitiveType final : public Type {
   public:
    ALASKA_NOCOPY_NOMOVE(PrimitiveType);
    PrimitiveType(llvm::Type *t)
        : Type(Type::PrimitiveTyID)
        , _wrapped(t) {}
    virtual ~PrimitiveType() = default;

    static bool classof(const Type *s) { return s->getTypeID() == Type::PrimitiveTyID; }
    llvm::Type *getWrapped(void) { return _wrapped; }

   private:
    llvm::Type *_wrapped = nullptr;
  };


  class StructType final : public Type {
   public:
    ALASKA_NOCOPY_NOMOVE(StructType);
    StructType(llvm::StringRef name, const std::vector<alaska::Type *> &fields)
        : Type(Type::StructTyID)
        , _name(name.bytes_begin(), name.bytes_end())
        , _fields(fields) {}

    virtual ~StructType() = default;

    static bool classof(const Type *s) { return s->getTypeID() == Type::StructTyID; }

    inline auto getFieldCount(void) { return _fields.size(); }
    inline auto &getFields(void) { return _fields; }
    inline const std::string &getName(void) { return _name; }

    inline bool validIndex(unsigned idx) const { return _fields.size() > idx; }
    inline auto *getTypeAtIndex(unsigned idx) const { return _fields.at(idx); }
    alaska::Type *getTypeAtIndex(const Value *V) const {
      unsigned Idx = (unsigned)cast<Constant>(V)->getUniqueInteger().getZExtValue();
      ALASKA_SANITY(validIndex(Idx), "Invalid structure index!");
      return getTypeAtIndex(Idx);
    }

   private:
    std::string _name;
    std::vector<alaska::Type *> _fields;
  };


  class PointerType final : public Type {
   public:
    ALASKA_NOCOPY_NOMOVE(PointerType);
    PointerType(alaska::Type *elTy)
        : Type(Type::PointerTyID)
        , _elementType(elTy) {}
    virtual ~PointerType() = default;

    static bool classof(const Type *s) { return s->getTypeID() == Type::PointerTyID; }

    inline auto *getElementType(void) { return _elementType; }

   private:
    alaska::Type *_elementType;
  };


  class ArrayType final : public Type {
   public:
    ALASKA_NOCOPY_NOMOVE(ArrayType);
    ArrayType(alaska::Type *elTy, uint64_t numElements)
        : Type(Type::ArrayTyID)
        , _elementType(elTy)
        , _numElements(numElements) {}
    virtual ~ArrayType() = default;

    static bool classof(const Type *s) { return s->getTypeID() == Type::ArrayTyID; }

    inline auto *getElementType(void) { return _elementType; }
    inline auto getNumElements(void) { return _numElements; }

   private:
    alaska::Type *_elementType;
    uint64_t _numElements;
  };


  class TILatticePoint : public LatticePoint<alaska::Type *> {
   public:
    using Base = LatticePoint<alaska::Type *>;
    using Base::Base;
    ~TILatticePoint() override = default;
    // Return true if the point changed.
    bool meet(const Base &incoming) override;
    bool join(const Base &incoming) override;
  };


  // A context in which the alaska types exist. This class encapuslates the conversion to alaska
  // types, as well as the assumptions which assign types to llvm values. At it's core, alaska
  // exists to map an LLVM type to a representation which is more friendly to the forms of analysis
  // we want to do. As such, each llvm type must be able to map into it's relative alaska type, which
  // has additional information we extract from optimistic analysis.
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

   private:
    alaska::PointerType *getPointerTo(alaska::Type *elTy);
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
  alaska::Type *getIndexedType(alaska::Type *Agg, ArrayRef<llvm::Value*> Idxs);


}  // namespace alaska
