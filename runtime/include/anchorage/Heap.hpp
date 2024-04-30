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

// This file contains the structures and methods which describe the
// Anchorage heap, segments, and the pages. The overall structure of
// the heap is very similar to that of `mimalloc` except the design of
// the low level `Page` structure, which is pluggable.
// NOTE: this heap design differs drastically to that outlined in the
// original Alaska paper.

// Anchorage's heap does not seek to fully replace `malloc` on a
// system, as it only concerns itself with handles. As a result, large
// objects are simply allocated by the system allocator (malloc). This
// simplifies the design substantially, as objects larger than a
// `Page` don't need special case handling. We decided this because
// objects larger than a certain point are probably better handled by
// virtual memory only, and don't really benefit from object movement.

namespace anchorage {

  constexpr uint64_t PageSize = 0;
  constexpr uint64_t PagesPerSegment = 0;
  constexpr uint64_t SegmentSize = PageSize * PagesPerSegment;


  // Heap:
  class Heap {
  };

  // Segment: A block of memory allocated in a contiguous, aligned, region of
  // the virtual address space which is subdivided into Pages which are managed
  // individually
  class Segment {};

  // Page: a slice of a segment, aligned to some value, which manages the
  // allocation of individual objects. It does not need to be size-segregated,
  // but by default is. Custom `Page` subclasses can be designed to permit
  // things like bump allocation or other things.
  class Page {};
};  // namespace anchorage
