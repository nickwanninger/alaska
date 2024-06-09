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



// TEST(SizeClass, RoundUpIsSane) {
//   for (int i = 0; i < alaska::num_size_classes; i++) {
//     size_t sz = alaska::class_to_size(i);
//     printf("%3d,%zu\n", i, sz);
//   }
// }


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
