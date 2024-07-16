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
#include "alaska/Logger.hpp"
#include "gtest/gtest.h"
#include <vector>
#include <alaska/Heap.hpp>


#include <stdlib.h>
#include <alaska/SizeClass.hpp>
#include <alaska/Runtime.hpp>




TEST(SizeClass, RoundUpIsSane) {
  // Iterate just up to 1MB for now.
  for (size_t sz = alaska::alignment; sz < 1048576; sz += alaska::alignment) {
    size_t rounded = alaska::round_up_size(sz);
    ASSERT_TRUE(rounded >= sz);
  }
}



TEST(SizeClass, ClassToSizeToClass) {
  for (int cl = 0; cl < alaska::num_size_classes; cl++) {
    size_t sz = alaska::class_to_size(cl);
    int cl2 = alaska::size_to_class(sz);
    ASSERT_EQ(cl, cl2);
  }
}

TEST(SizeClass, SizeToClass) {
  // Iterate just up to 1MB for now.
  for (size_t sz = alaska::alignment; sz < 1048576; sz += alaska::alignment) {
    int cl = alaska::size_to_class(sz);
    ASSERT_FALSE(sz > alaska::class_to_size(cl));
  }
}

TEST(SizeClass, ClassToSize) {
  for (int cl = 0; cl < alaska::num_size_classes; cl++) {
    size_t sz = alaska::class_to_size(cl);
    ASSERT_FALSE(cl != alaska::size_to_class(sz));
  }
}


void printBitPattern(uint64_t n) {
  int bits = 64;
  for (long i = bits - 1; i >= 0; i--) {
    printf("%ld", (n >> i) & 1);
  }
}

extern void dump_sc_shit(size_t size);

TEST(SizeClass, ClassToSizeTwo) {
  // for (size_t size = 1; size < MI_LARGE_OBJ_SIZE_MAX * sizeof(uintptr_t);
  // size++) {
  //   int bin = mi_bin(size);
  //   printf("%4d ", bin);
  //   size_t size_r = mi_bin_size(bin);
  //   printf("%4d ", mi_bin(size));
  //   printBitPattern(size);
  //   printf(" %16zu ", size);
  //   printf(" %16zu ", size_r);
  //   printf("\n");
  // }


  size_t previous_blocks = 1;
  for (int bin = 0; bin < alaska::num_size_classes; bin++) {
    size_t size = alaska::class_to_size(bin);

    dump_sc_shit(size);
    printf(" ");
    printf("bin:%2d / %2d ", bin, alaska::size_to_class(size));
    printf("sz: %8zu ", size);

    size_t blocks = size / 16;
    printf("%8zu ", size / 16);

    printf("%f ", blocks / (double)previous_blocks);

    previous_blocks = blocks;


    printf("\n");
  }
}
