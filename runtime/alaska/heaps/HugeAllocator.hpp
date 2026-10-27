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
#include <ck/set.h>

namespace alaska {

  // Placed just before every huge allocation. free() recovers it by subtracting
  // sizeof(HugeHeader) from the caller's pointer. The run starts data_offset bytes
  // before the caller's pointer (more than sizeof(HugeHeader) for aligned allocations).
  struct HugeHeader {
    size_t total_size;     // full size of the run or mapping, page-aligned
    uint32_t data_offset;  // bytes from the start of the run to the caller's pointer
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
    // `align` must be a power of two. The result is aligned to at least 16 bytes.
    void *alloc_aligned(size_t size, size_t align);
    // Returns false and does nothing if this allocator does not own `ptr`.
    bool free(void *ptr);
    size_t usable_size(void *ptr);

    // True if `ptr` came from alloc() and is not freed yet. A pointer from
    // another allocator (for example glibc) is never owned.
    bool owns(void *ptr);

   private:
    bool in_region(void *ptr);
    void *place(void *base, size_t total, size_t data_offset, bool direct);
    void *alloc_page_run(size_t total);
    void free_page_run(void *ptr, size_t total);
    void *alloc_direct_mmap(size_t total);
    void *alloc_direct_mmap_aligned(size_t size, size_t align);
    void free_direct_mmap(void *ptr, size_t total);

    void *region_base = nullptr;
    size_t region_size = 0;
    size_t total_pages = 0;

    uint64_t *alloc_bitmap = nullptr;   // 1 bit per 4KB page (1=allocated, 0=free)
    uint32_t *run_lengths = nullptr;    // page count stored at the start index of each run

    size_t search_hint = 0;             // scan starting point, avoids rescanning known-full pages
    ck::set<uintptr_t> direct_maps;     // user pointers of live direct-mmap allocations
    ck::mutex lock;
  };

}  // namespace alaska
