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


#include <alaska.h>
#include <alaska/alaska.hpp>

#define KB 1024UL
#define MB KB * 1024UL
#define GB MB * 1024UL

namespace anchorage {


  // main interface to the allocator
  void allocator_init(void);
  void allocator_deinit(void);

  constexpr size_t page_size = 4096;
  // Maximum object size that anchorage will handle (internally)
  static constexpr size_t maxHandleSize = 16384;
  static constexpr size_t defaultArenaSize = 64 * GB;
  static constexpr unsigned extentClassCount = 256;
  static constexpr size_t classSizesMax = 25;

  // A base and length in *pages*
  struct Extent {
    // offset and length are in pages
    explicit Extent(uint32_t _offset, uint32_t _length)
        : offset(_offset)
        , length(_length) {
    }

    Extent(const Extent &rhs)
        : offset(rhs.offset)
        , length(rhs.length) {
    }

    Extent &operator=(const Extent &rhs) {
      offset = rhs.offset;
      length = rhs.length;
      return *this;
    }

    Extent(Extent &&rhs)
        : offset(rhs.offset)
        , length(rhs.length) {
    }

    bool empty() const {
      return length == 0;
    }

    // reduce the size of this extent to pageCount, return another extent
    // with the rest of the pages.
    Extent splitAfter(size_t pageCount) {
      auto restPageCount = length - pageCount;
      length = pageCount;
      return Extent(offset + pageCount, restPageCount);
    }

    uint32_t extentClass() const {
      uint32_t out = anchorage::extentClassCount;
      if (length < out) out = length;
      return out - 1;
    }

    size_t byteLength() const {
      return length * page_size;
    }

    inline bool operator==(const Extent &rhs) {
      return offset == rhs.offset && length == rhs.length;
    }

    inline bool operator!=(const Extent &rhs) {
      return !(*this == rhs);
    }

    uint32_t offset; // offset from the *start of the heap*
    uint32_t length; // length in pages
  };

}  // namespace anchorage
