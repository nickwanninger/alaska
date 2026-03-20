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
#include <stdint.h>

namespace alaska {

  template <typename T>
  void lphashset_init(T *set, size_t size) {
    memset(set, 0xFF, size * sizeof(T));
  }

  template <typename T>
  bool lphashset_insert(T *set, size_t size, T key) {
    T index = key % size;
    uint64_t start_index = index;

    while (set[index] != 0xFFFFFFFFFFFFFFFFUL) {
      if (set[index] == key) {
        return false;  // Already exists
      }
      index = (index + 1) % size;  // Linear probing
      if (index == start_index) {
        return false;  // Table full (shouldn't happen if TABLE_SIZE is large enough)
      }
    }

    set[index] = key;
    return true;
  }

  template <typename T>
  bool lphashset_contains(T *set, size_t size, T key) {
    T index = key % size;
    uint64_t start_index = index;

    while (set[index] != 0xFFFFFFFFFFFFFFFFUL) {
      if (set[index] == key) {
        return true;
      }
      index = (index + 1) % size;  // Linear probing
      if (index == start_index) {
        return false;  // Table full, key not found
      }
    }

    return false;
  }


  template <typename T>
  size_t lphashset_count(T *set, size_t size) {
    size_t count = 0;
    for (size_t i = 0; i < size; ++i) {
      if (set[i] != 0xFFFFFFFFFFFFFFFFUL) {
        count += 1;
      }
    }
    return count;
  }
}  // namespace alaska
