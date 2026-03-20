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

#include <stddef.h>
#include <stdint.h>
#include <ck/lock.h>

namespace alaska {

  struct HugeHeader {
    size_t total_size;     // includes header + padding
    bool is_direct_mmap;   // true = individually mmap'd
  };

  class HugeAllocator {
   public:
    static constexpr size_t PAGE_SIZE = 4096;
    static constexpr size_t DIRECT_MMAP_THRESH = 256 * 1024;
    static constexpr size_t DEFAULT_REGION_SIZE = 1UL * 1024 * 1024 * 1024;  // 1GB

    HugeAllocator();
    ~HugeAllocator();

    void *alloc(size_t size);
    void free(void *ptr);
    size_t usable_size(void *ptr);
    bool contains(void *ptr);

   private:
    void *alloc_page_run(size_t total);
    void free_page_run(void *ptr, size_t total);
    void *alloc_direct_mmap(size_t total);
    void free_direct_mmap(void *ptr, size_t total);

    void *region_base = nullptr;
    size_t region_size = 0;
    size_t total_pages = 0;

    uint64_t *alloc_bitmap = nullptr;   // 1 bit per page (1=allocated)
    uint32_t *run_lengths = nullptr;    // pages-in-run at each run's start index

    size_t search_hint = 0;
    ck::mutex lock;
  };

}  // namespace alaska
