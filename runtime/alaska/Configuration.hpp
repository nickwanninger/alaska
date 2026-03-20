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

#include <stdint.h>

namespace alaska {
  // This structure is threaded through the creation of the runtime to
  // allow configuration of various features.
  struct Configuration {
    uintptr_t handle_table_location =
        (0x8000000000000000LLU >> (ALASKA_SIZE_BITS - ALASKA_SQUEEZE_BITS));
  };
}  // namespace alaska
