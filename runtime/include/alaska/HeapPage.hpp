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


#include <stdlib.h>
#include <alaska/alaska.hpp>


namespace alaska {


  static constexpr uint64_t page_shift_factor = 17;
  static constexpr size_t page_size = 1LU << page_shift_factor;

  // A super simple type-level indicator that a size is aligned to the heap's alignment
  class AlignedSize final {
    size_t size;

   public:
    AlignedSize(size_t size)
        : size((size + 15) & ~15) {}
    size_t operator*(void) { return size; }
    operator size_t(void) { return size; }
    AlignedSize operator+(size_t other) { return size + other; }
  };

  // This class is the base-level class for a heap page. A heap page is a
  // single contiguous block of memory that is managed by some policy.
  class HeapPage {
   public:
    virtual ~HeapPage() {}
    // The size argument is already aligned and rounded up to a multiple of the rounding size.
    // Returns the data allocated, or NULL if it couldn't be.
    virtual void* alloc(const Mapping& m, AlignedSize size) = 0;
    virtual bool release(Mapping& m, void* ptr) = 0;
  };
}  // namespace alaska
