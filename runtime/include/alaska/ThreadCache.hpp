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

#include <alaska/Heap.hpp>

namespace alaska {

  struct Runtime;

  // A ThreadCache is the class which manages the thread-private
  // allocations out of a shared heap. The core runtime itself does
  // *not* manage where thread caches are used. It is assumed
  // something else manages storing a pointer to a ThreadCache in some
  // thread-local variable
  class ThreadCache final {
   public:
    ThreadCache(alaska::Runtime &rt)
    : runtime(rt) {}

    void *halloc(size_t size, bool zero = false);
    void hfree(void *ptr);


   private:
    alaska::Runtime &runtime;
  };



}  // namespace alaska
