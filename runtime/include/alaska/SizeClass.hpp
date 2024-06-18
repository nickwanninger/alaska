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

#include <unistd.h>
#include <alaska/HeapPage.hpp>

namespace alaska {
  namespace internal {
    // Helper function to statically compute integer logarithms.
    template <size_t BaseNumerator, size_t BaseDenominator, size_t Value>
    class ilog;

    template <size_t BaseNumerator, size_t BaseDenominator>
    class ilog<BaseNumerator, BaseDenominator, 1> {
     public:
      enum { VALUE = 0 };
    };

    template <size_t BaseNumerator, size_t BaseDenominator, size_t Value>
    class ilog {
     public:
      enum {
        VALUE =
            1 +
            ilog<BaseNumerator, BaseDenominator, (Value * BaseDenominator) / BaseNumerator>::VALUE
      };
    };
  }  // namespace internal


  // ...
  static constexpr long alignment = 16; // Byte alignment for size classes
  static constexpr long max_overhead = 20;  // percent internal fragmentation
  // static constexpr long max_object_size = (1LU << 30);
  static constexpr long max_object_size = (alaska::page_size / 2);
  static constexpr long num_size_classes = internal::ilog<100 + max_overhead, 100, max_object_size>::VALUE;

  // Returns size of the memory block that will be allcoated if you ask for `sz` bytes.
  size_t round_up_size(size_t sz);

  int size_to_class(size_t sz);
  size_t class_to_size(int cls);

}  // namespace alaska
