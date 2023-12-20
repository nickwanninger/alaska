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



  struct Block;
  struct Chunk;

  // main interface to the allocator
  void allocator_init(void);
  void allocator_deinit(void);

  constexpr size_t page_size = 4096;
  // Maximum object size that anchorage will handle (internally)
  static constexpr size_t maxHandleSize = 16384;
  static constexpr size_t defaultArenaSize = 512 * GB;
  static constexpr unsigned extentClassCount = 256;
  static constexpr size_t classSizesMax = 25;


  constexpr size_t block_size = 16;
  constexpr size_t min_chunk_pages = (512 * GB) / 4096;

  // constexpr size_t min_chunk_pages = 4096;
  static inline size_t size_with_overhead(size_t sz) {
    return sz + block_size;
  }


	double get_heap_frag(void);
	double get_heap_frag_locked(void);
}  // namespace anchorage
