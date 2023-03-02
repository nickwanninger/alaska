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


  struct Block;
  struct Chunk;

  // main interface to the allocator
  void *alloc(alaska::Mapping &mapping, size_t size);
  void free(alaska::Mapping &mapping, void *ptr);
  void barrier(bool force = false);

  void allocator_init(void);
  void allocator_deinit(void);

  constexpr size_t block_size = 16;
  constexpr size_t page_size = 4096;
  constexpr size_t min_chunk_pages = 1024;
  static inline size_t size_with_overhead(size_t sz) {
    return sz + block_size;
  }

  extern size_t moved_bytes;  // How many bytes have been moved by anchorage?
  void *memmove(void *dst, void *src, size_t size);
}  // namespace anchorage
