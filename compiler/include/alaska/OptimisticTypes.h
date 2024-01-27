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

#include <alaska/LatticePoint.h>
#include <alaska/Utils.h>
#include <map>

/**
 * The purpose of this file is to provide an interface for extracting some
 * ``optimistic'' type from the uses of different opaque pointers.
 */

namespace alaska {
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
