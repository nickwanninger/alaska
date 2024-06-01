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
#include <stdlib.h>

namespace alaska {
  static constexpr size_t kilobyte = 1024;
  static constexpr size_t megabyte = 1024 * kilobyte;
  static constexpr size_t gigabyte = 1024 * kilobyte;

  // For now, the heap is a fixed size, large contiguious
  // block of memory managed by the PageManager. Eventually,
  // we will split it up into different mmap regions, but that's
  // just a problem solved by another layer of allocators.
  static constexpr size_t heap_size = 128 * gigabyte;

  // The PageManager is responsible for managing the memory backing the heap, and subdividing it
  // into pages which are handed to HeapPage instances.
  class PageManager final {
    // This is the memory backing the heap. It is `alaska::heap_size` bytes long.
    void *heap;

   public:
    PageManager();
    ~PageManager();


   private:
    // Get a page from the page manager given it's index.
    // This may return NULL, in which case a new one can be allocated.
    HeapPage *get_page(size_t index);
  };

  class Heap final {
    // A heap is basically just a wrapper around a page manager.
    PageManager pm;

   public:
  };
}  // namespace alaska