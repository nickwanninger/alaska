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

#include <gtest/gtest.h>
#include <alaska.h>
#include "alaska/heaps/HeapPage.hpp"
#include "alaska/util/Logger.hpp"
#include "gtest/gtest.h"
#include <vector>
#include <alaska/heaps/Heap.hpp>


#include <stdlib.h>
#include <alaska/heaps/SizeClass.hpp>
#include <alaska/core/Runtime.hpp>




TEST(SizeClass, RoundUpIsSane) {
  // Iterate just up to 1MB for now.
  for (size_t sz = alaska::alignment; sz < alaska::huge_object_thresh; sz += alaska::alignment) {
    size_t rounded = alaska::round_up_size(sz);
    // alaska::printf("%lu -> %lu   %ld\n", sz, rounded, rounded - sz);
    ASSERT_TRUE(rounded >= sz);
  }
}



TEST(SizeClass, ClassToSizeToClass) {
  for (alaska::size_class_t cl = 0; cl < alaska::num_size_classes; cl++) {
    size_t sz = alaska::class_to_size(cl);
    if (sz >= alaska::huge_object_thresh) continue;
    int cl2 = alaska::size_to_class(sz);
    // alaska::printf("%lu -> %lu -> %lu\n", cl, sz, cl2);
    ASSERT_EQ(cl, cl2);
  }
}

TEST(SizeClass, SizeToClass) {
  // Iterate just up to 1MB for now.
  for (size_t sz = alaska::alignment; sz < alaska::huge_object_thresh; sz += alaska::alignment) {
    int cl = alaska::size_to_class(sz);
    ASSERT_FALSE(sz > alaska::class_to_size(cl));
  }
}

TEST(SizeClass, ClassToSize) {
  for (alaska::size_class_t cl = 0; cl < alaska::num_size_classes; cl++) {
    size_t sz = alaska::class_to_size(cl);
    if (sz >= alaska::huge_object_thresh) continue;
    ASSERT_FALSE(cl != alaska::size_to_class(sz));
  }
}


void printBitPattern(uint64_t n) {
  int bits = 64;
  for (long i = bits - 1; i >= 0; i--) {
    printf("%ld", (n >> i) & 1);
  }
}

TEST(SizeClass, ProblematicSizes) {
  // Debug test for sizes 8, 28, and 40 that appeared in segfault output
  printf("\n\nSize Class Debug for Problematic Sizes:\n");
  printf("Alignment: %lu bytes\n", alaska::alignment);
  printf("Small object alignment: %lu bytes\n", alaska::small_word_size);
  printf("Large object alignment: %lu bytes\n\n", alaska::large_word_size);
  
  const size_t test_sizes[] = {8, 28, 40};
  
  for (size_t sz : test_sizes) {
    alaska::size_class_t cls = alaska::size_to_class(sz);
    size_t rounded = alaska::class_to_size(cls);
    printf("  Size %2lu -> class %3lu -> rounded size %2lu%s\n", 
           sz, cls, rounded, (sz != rounded) ? " (MISMATCH!)" : "");
  }
  
  printf("\nFirst 10 size classes:\n");
  for (int i = 0; i < 10; i++) {
    size_t sz = alaska::class_to_size(i);
    printf("  class %2d -> size %3lu\n", i, sz);
  }
}
