/*
 * This file is part of the Alaska Handle-Based Memory Management System
 *
 * Copyright (c) 2023, Nick Wanninger <ncw@u.northwestern.edu>
 * Copyright (c) 2023, The Constellation Project
 * All rights reserved.
 *
 * This is free software.  You are permitted to use, redistribute,
 * and modify it as specified in the file "LICENSE".
 */
#pragma once

#include <ck/set.h>
#include <anchorage/Anchorage.hpp>

namespace anchorage {

  /**
   * Space:
   */
  class Space {
   public:
    auto *older(void) const {
      return m_older;
    }

    auto age(void) const {
      return m_age;
    }

    Space(int depth);

    void dump();

   private:
    int m_age;
    // The next (older) space
    // Data in spaces only moves towards older ones,
    // so older spaces do not point to younger spaces.
    Space *m_older = nullptr;
  };


  void initialize_spaces(int num_spaces = 2);
}  // namespace anchorage