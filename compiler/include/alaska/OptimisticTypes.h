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

#include "llvm/IR/Function.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/Support/raw_ostream.h"
#include <alaska/Utils.h>
#include <vector>
#include <map>

/**
 * The purpose of this file is to provide an interface for extracting some
 * ``optimistic'' type from the uses of different opaque pointers.
 */

namespace alaska {

  enum class PointType { Overdefined, Defined, Underdefined };
  // TODO: move to a new header
  template <typename StateT>
  class LatticePoint {
   public:
    LatticePoint() = default;
    LatticePoint(PointType type, StateT state = {})
        : type(type)
        , m_state(state) {
      if (state == NULL) {
        ALASKA_SANITY(!is_defined(), "DIE");
      } else {
        ALASKA_SANITY(is_defined(), "DIE2");
      }
    }

    virtual ~LatticePoint() = default;

    inline bool is_defined(void) const { return type == PointType::Defined; }
    inline bool is_overdefined(void) const { return type == PointType::Overdefined; }
    inline bool is_underdefined(void) const { return type == PointType::Underdefined; }
    inline auto get_type(void) const { return type; }

    void set_state(StateT state) {
      m_state = state;
      type = PointType::Defined;
    }
    StateT get_state(void) const {
      ALASKA_SANITY(is_defined(), "Must be defined to call get_state()");
      return m_state;
    }


    inline void set_overdefined(void) {
      this->m_state = {};
      this->type = PointType::Overdefined;
    }
    inline void set_underdefined(void) {
      this->m_state = {};
      this->type = PointType::Underdefined;
    }

    // Return true if the point changed.
    virtual bool meet(const LatticePoint<StateT> &incoming) = 0;
    virtual bool join(const LatticePoint<StateT> &incoming) = 0;


    friend llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const LatticePoint<StateT> &LP) {
      if (LP.is_underdefined()) {
        os << "⊥";
      } else if (LP.is_overdefined()) {
        os << "⊤";
      } else {
        os << " " << *LP.get_state();
      }
      return os;
    }

   private:
    PointType type = PointType::Underdefined;
    StateT m_state = {};
  };




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




  class OTLatticePoint : public LatticePoint<llvm::Type *> {
   public:
    using Base = LatticePoint<llvm::Type *>;
    using Base::Base;
    ~OTLatticePoint() override = default;
    // Return true if the point changed.
    bool meet(const Base &incoming) override;
    bool join(const Base &incoming) override;
  };

  class OptimisticTypes : public llvm::InstVisitor<OptimisticTypes> {
   public:
    explicit OptimisticTypes(llvm::Function &F);
    // explicit OptimisticTypes(llvm::Module &M); // TODO
    OptimisticTypes() = default;


    void analyze(llvm::Function &F);
    // void analyze(llvm::Module &M); // TODO
    void dump();

    OTLatticePoint get_lattice_point(llvm::Value *v);

   protected:
    friend llvm::InstVisitor<OptimisticTypes>;
    void visitGetElementPtrInst(llvm::GetElementPtrInst &I);
    void visitLoadInst(llvm::LoadInst &I);
    void visitStoreInst(llvm::StoreInst &I);

    void use(llvm::Value *v, llvm::Type *t);


   private:
    std::map<llvm::Value *, OTLatticePoint> m_types;
  };
}  // namespace alaska
