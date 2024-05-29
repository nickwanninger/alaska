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


#include <alaska/HeapPage.hpp>
#include <ck/vec.h>

namespace alaska {

  // The Heap manages HeapPages, and is responsible for allocating and
  // deallocating memory from the system. The heap is owned by a runtime, but
  // many of them can *technically* exist at once.
  class Heap final {
    ck::vec<HeapPage*> pages;

   public:
  };
}  // namespace alaska