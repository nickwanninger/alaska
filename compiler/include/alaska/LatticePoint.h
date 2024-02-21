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

#include "llvm/Support/raw_ostream.h"
#include <alaska/Utils.h>

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
        ALASKA_SANITY(!is_defined(), "Must not be defined if state is null");
      } else {
        ALASKA_SANITY(is_defined(), "Must be defined if state isn't null");
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
        os << *LP.get_state();
      }
      return os;
    }

   private:
    PointType type = PointType::Underdefined;
    StateT m_state = {};
  };

}  // namespace alaska
