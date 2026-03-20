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

  // Prepended to every huge allocation. The caller receives a pointer
  // just past this header; free() recovers it by subtracting sizeof(HugeHeader).
  struct HugeHeader {
    size_t total_size;     // full allocation size including this header, page-aligned
    bool is_direct_mmap;   // which deallocation path to take
  };

  // VM-based allocator for objects that exceed the sized-page heap (>= max_large_size).
  // Returns raw pointers, not handles.
  //
  // Two tiers:
  //   1. Page-run (< 256KB): A 1GB virtual region reserved with MAP_NORESERVE (no physical
  //      memory until touched). Allocations find consecutive free 4KB pages via bitmap scan.
  //      Freed pages are returned to the OS with MADV_DONTNEED but the virtual region persists.
  //   2. Direct mmap (>= 256KB): Each allocation gets its own mmap/munmap pair. The syscall
  //      cost is negligible relative to the allocation size at this point.
  //
  // If the page-run region fills up or fragments, allocations transparently fall back to
  // direct mmap — the region is an optimization, not a hard limit.
  //
  // A single mutex protects the bitmap. This is acceptable because huge allocations are
  // already behind unlikely() guards on the allocation hot path.
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

    // Only returns true for pointers within the page-run region.
    // Direct-mmap pointers can't be range-checked; callers should use
    // !heap.contains(ptr) as the discriminator (which is the existing pattern).
    bool contains(void *ptr);

   private:
    void *alloc_page_run(size_t total);
    void free_page_run(void *ptr, size_t total);
    void *alloc_direct_mmap(size_t total);
    void free_direct_mmap(void *ptr, size_t total);

    void *region_base = nullptr;
    size_t region_size = 0;
    size_t total_pages = 0;

    uint64_t *alloc_bitmap = nullptr;   // 1 bit per 4KB page (1=allocated, 0=free)
    uint32_t *run_lengths = nullptr;    // page count stored at the start index of each run

    size_t search_hint = 0;             // scan starting point, avoids rescanning known-full pages
    ck::mutex lock;
  };

}  // namespace alaska
