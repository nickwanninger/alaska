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
#include <alaska/internal.h>

namespace anchorage {

  // main interface to the allocator
  void allocator_init(void);
  void allocator_deinit(void);

  constexpr size_t block_size = 16;
  constexpr size_t page_size = 4096;
  // How many pages should a chunk be?
  // TODO: This needs to be tuned dynamically
  // constexpr size_t min_chunk_pages = 16384 * 2;  // 64mb of pages
  constexpr size_t min_chunk_pages = 16384;
  static inline size_t size_with_overhead(size_t sz) {
    return sz + block_size;
  }

  // Maximum object size that anchorage will handle (internally)
  static constexpr size_t kMaxSize = 16384;
  static constexpr size_t kArenaSize = 64ULL * 1024ULL * 1024ULL * 1024ULL;  // 64 GB

  static constexpr size_t kClassSizesMax = 25;


}  // namespace anchorage
