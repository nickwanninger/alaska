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

#include <stdint.h>
#include <stdlib.h>

#include <anchorage/Anchorage.hpp>

namespace anchorage {

  // from tcmalloc/gperftools
  class SizeMap {
   private:
    //-------------------------------------------------------------------
    // Mapping from size to size_class and vice versa
    //-------------------------------------------------------------------

    // Sizes <= 1024 have an alignment >= 8.  So for such sizes we have an
    // array indexed by ceil(size/8).  Sizes > 1024 have an alignment >= 128.
    // So for these larger sizes we have an array indexed by ceil(size/128).
    //
    // We flatten both logical arrays into one physical array and use
    // arithmetic to compute an appropriate index.  The constants used by
    // ClassIndex() were selected to make the flattening work.
    //
    // Examples:
    //   Size       Expression                      Index
    //   -------------------------------------------------------
    //   0          (0 + 7) / 8                     0
    //   1          (1 + 7) / 8                     1
    //   ...
    //   1024       (1024 + 7) / 8                  128
    //   1025       (1025 + 127 + (120<<7)) / 128   129
    //   ...
    //   32768      (32768 + 127 + (120<<7)) / 128  376
    static const int kMaxSmallSize = 1024;
    static const size_t kClassArraySize = ((maxHandleSize + 127 + (120 << 7)) >> 7) + 1;
    static const unsigned char class_array_[kClassArraySize];

    static inline size_t SmallSizeClass(size_t s) {
      return (static_cast<uint32_t>(s) + 7) >> 3;
    }

    static inline size_t LargeSizeClass(size_t s) {
      return (static_cast<uint32_t>(s) + 127 + (120 << 7)) >> 7;
    }

    // If size is no more than kMaxSize, compute index of the
    // class_array[] entry for it, putting the class index in output
    // parameter idx and returning true. Otherwise return false.
    static inline bool ALASKA_INLINE ClassIndexMaybe(size_t s, uint32_t *idx) {
      if (likely(s <= kMaxSmallSize)) {
        *idx = (static_cast<uint32_t>(s) + 7) >> 3;
        return true;
      } else if (s <= maxHandleSize) {
        *idx = (static_cast<uint32_t>(s) + 127 + (120 << 7)) >> 7;
        return true;
      }
      return false;
    }

    // Compute index of the class_array[] entry for a given size
    static inline size_t ClassIndex(size_t s) {
      // Use unsigned arithmetic to avoid unnecessary sign extensions.
      // assert(s <= kMaxSize);
      if (likely(s <= kMaxSmallSize)) {
        return SmallSizeClass(s);
      } else {
        return LargeSizeClass(s);
      }
    }

    // Mapping from size class to max size storable in that class
    static const int32_t class_to_size_[classSizesMax];

   public:
    static constexpr size_t num_size_classes = 25;


    static inline int SizeClass(size_t size) {
      return class_array_[ClassIndex(size)];
    }

    // Check if size is small enough to be representable by a size
    // class, and if it is, put matching size class into *cl. Returns
    // true iff matching size class was found.
    static inline bool ALASKA_INLINE GetSizeClass(size_t size, uint32_t *cl) {
      uint32_t idx;
      if (!ClassIndexMaybe(size, &idx)) {
        return false;
      }
      *cl = class_array_[idx];
      return true;
    }

    // Get the byte-size for a specified class
    // static inline int32_t ALASKA_INLINE ByteSizeForClass(uint32_t cl) {
    static inline size_t ALASKA_INLINE ByteSizeForClass(int32_t cl) {
      return class_to_size_[static_cast<uint32_t>(cl)];
    }

    // Mapping from size class to max size storable in that class
    static inline int32_t class_to_size(uint32_t cl) {
      return class_to_size_[cl];
    }
  };

};  // namespace anchorage