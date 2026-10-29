/*
 * This file is part of the Alaska Handle-Based Memory Management System
 *
 * Copyright (c) 2026, Nick Wanninger <ncw@u.northwestern.edu>
 * Copyright (c) 2026, The Constellation Project
 * All rights reserved.
 *
 * This is free software.  You are permitted to use, redistribute,
 * and modify it as specified in the file "LICENSE".
 */

#pragma once

#include <stddef.h>

// Allocation events for the access trace (rt/access_trace.cpp).
namespace alaska::access_trace {
  void alloc(void *result, size_t size);
  void free(void *ptr);
}  // namespace alaska::access_trace
