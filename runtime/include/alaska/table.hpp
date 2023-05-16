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


#include <alaska/internal.h>

namespace alaska {
  namespace table {
    // Allocate a table entry, returning a pointer to the mapping.
    auto get(void) -> alaska::Mapping *;
    // Release a table entry, allowing it to be used again in the future.
    auto put(alaska::Mapping *) -> void;

    // Allow iterating over each table entry. Typical C++ iteration
    auto begin(void) -> alaska::Mapping *;
    auto end(void) -> alaska::Mapping *;

    // Initialization and deinitialization
    void init();
    void deinit();

  }  // namespace table
}  // namespace alaska