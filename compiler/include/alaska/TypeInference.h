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
#include <alaska/Types.h>

namespace alaska {
  class TILatticePoint : public LatticePoint<alaska::Type *> {
   public:
    using Base = LatticePoint<alaska::Type *>;
    using Base::Base;
    ~TILatticePoint() override = default;
    // Return true if the point changed.
    bool meet(const Base &incoming) override;
    bool join(const Base &incoming) override;
  };

}  // namespace alaska
