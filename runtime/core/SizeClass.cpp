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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <alaska/SizeClass.hpp>
#include <alaska/utils.h>

#include <math.h>



/// Builds an array to speed size computations, stored in sizes.
static bool create_table(size_t *sizes) {
  const double base = (1.0 + (double)alaska::max_overhead / (double)100.0);
  size_t sz = alaska::alignment;
  for (int i = 0; i < alaska::num_size_classes; i++) {
    sizes[i] = sz;
    size_t newSz = (size_t)(floor((double)base * (double)sz));
    newSz = newSz - (newSz % alaska::alignment);
    while ((double)newSz / (double)sz < base) {
      newSz += alaska::alignment;
    }
    sz = newSz;
  }
  return true;
}

static size_t sizes[alaska::num_size_classes];
static bool tables_created = false;

namespace alaska {


  int size_to_class(size_t sz) {
    // Do a binary search to find the right size class.
    int left = 0;
    int right = alaska::num_size_classes - 1;
    while (left < right) {
      int mid = (left + right) / 2;
      if (class_to_size(mid) < sz) {
        left = mid + 1;
      } else {
        right = mid;
      }
    }
    ALASKA_ASSERT(class_to_size(left) >= sz, "Size class should be >= requested size");
    ALASKA_ASSERT((left == 0) || (class_to_size(left - 1) < sz), "Sanity");
    return left;
  }

  size_t class_to_size(int cls) {
    if (!tables_created) {
      create_table(sizes);
      tables_created = true;
    }
    return sizes[cls];
  }


  size_t round_up_size(size_t sz) {
    // Simply ask for the size
    return class_to_size(size_to_class(sz));
  }

}  // namespace alaska