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

#include <alaska/heaps/HugeAllocator.hpp>
#include <sys/mman.h>
#include <string.h>

namespace alaska {

  static size_t align_up(size_t val, size_t align) {
    return (val + align - 1) & ~(align - 1);
  }

  HugeAllocator::HugeAllocator() {
    region_size = DEFAULT_REGION_SIZE;
    total_pages = region_size / PAGE_SIZE;

    // Reserve virtual address space only
    region_base = mmap(nullptr, region_size, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (region_base == MAP_FAILED) {
      region_base = nullptr;
      region_size = 0;
      total_pages = 0;
      return;
    }

    // Metadata is demand-paged too: 32KB for bitmap, ~1MB for run_lengths on a 1GB region.
    size_t bitmap_bytes = (total_pages + 63) / 64 * sizeof(uint64_t);
    size_t run_lengths_bytes = total_pages * sizeof(uint32_t);

    alloc_bitmap = (uint64_t *)mmap(nullptr, bitmap_bytes, PROT_READ | PROT_WRITE,
                                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    run_lengths = (uint32_t *)mmap(nullptr, run_lengths_bytes, PROT_READ | PROT_WRITE,
                                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  }

  HugeAllocator::~HugeAllocator() {
    if (region_base) {
      munmap(region_base, region_size);
    }
    if (alloc_bitmap) {
      size_t bitmap_bytes = (total_pages + 63) / 64 * sizeof(uint64_t);
      munmap(alloc_bitmap, bitmap_bytes);
    }
    if (run_lengths) {
      size_t run_lengths_bytes = total_pages * sizeof(uint32_t);
      munmap(run_lengths, run_lengths_bytes);
    }
  }

  void *HugeAllocator::alloc(size_t size) {
    if (size == 0) return nullptr;

    size_t total = align_up(sizeof(HugeHeader) + size, PAGE_SIZE);

    if (total >= DIRECT_MMAP_THRESH || region_base == nullptr) {
      return alloc_direct_mmap(total);
    }

    void *result = alloc_page_run(total);
    if (result == nullptr) {
      return alloc_direct_mmap(total);
    }
    return result;
  }

  void HugeAllocator::free(void *ptr) {
    if (ptr == nullptr) return;

    auto *header = (HugeHeader *)((char *)ptr - sizeof(HugeHeader));

    if (header->is_direct_mmap) {
      free_direct_mmap(header, header->total_size);
    } else {
      free_page_run(header, header->total_size);
    }
  }

  size_t HugeAllocator::usable_size(void *ptr) {
    if (ptr == nullptr) return 0;
    auto *header = (HugeHeader *)((char *)ptr - sizeof(HugeHeader));
    return header->total_size - sizeof(HugeHeader);
  }

  bool HugeAllocator::contains(void *ptr) {
    if (ptr == nullptr) return false;

    auto addr = (uintptr_t)ptr;
    auto base = (uintptr_t)region_base;
    if (addr >= base && addr < base + region_size) return true;

    // For direct mmap'd allocations, recover the header and check the flag.
    // We can't range-check arbitrary mmap'd regions, so we don't claim
    // ownership of direct-mmap pointers here. The caller should use
    // !heap.contains(ptr) as the discriminator (which is the existing pattern).
    return false;
  }

  // --- Page-run allocator ---

  static inline bool bitmap_test(uint64_t *bitmap, size_t idx) {
    return (bitmap[idx / 64] >> (idx % 64)) & 1;
  }

  static inline void bitmap_set(uint64_t *bitmap, size_t idx) {
    bitmap[idx / 64] |= (1UL << (idx % 64));
  }

  static inline void bitmap_clear(uint64_t *bitmap, size_t idx) {
    bitmap[idx / 64] &= ~(1UL << (idx % 64));
  }

  // Scan the bitmap for `pages_needed` consecutive free pages, mark them, and
  // return a pointer into the region. Starts from search_hint and wraps around.
  // Returns nullptr if no run is found (caller falls back to direct mmap).
  void *HugeAllocator::alloc_page_run(size_t total) {
    size_t pages_needed = total / PAGE_SIZE;

    lock.lock();

    size_t start = search_hint;
    size_t run = 0;
    size_t scan_start = start;

    for (size_t i = start; i < total_pages; i++) {
      if (bitmap_test(alloc_bitmap, i)) {
        run = 0;
        scan_start = i + 1;
      } else {
        run++;
        if (run == pages_needed) {
          // Found a run starting at scan_start.
          for (size_t j = scan_start; j <= i; j++) {
            bitmap_set(alloc_bitmap, j);
          }
          run_lengths[scan_start] = pages_needed;
          search_hint = i + 1;
          lock.unlock();

          auto *header = (HugeHeader *)((char *)region_base + scan_start * PAGE_SIZE);
          header->total_size = total;
          header->is_direct_mmap = false;
          return (char *)header + sizeof(HugeHeader);
        }
      }
    }

    // Wrap around: scan from 0 to original search_hint.
    if (start > 0) {
      run = 0;
      scan_start = 0;
      size_t limit = start < total_pages ? start : total_pages;
      for (size_t i = 0; i < limit; i++) {
        if (bitmap_test(alloc_bitmap, i)) {
          run = 0;
          scan_start = i + 1;
        } else {
          run++;
          if (run == pages_needed) {
            for (size_t j = scan_start; j <= i; j++) {
              bitmap_set(alloc_bitmap, j);
            }
            run_lengths[scan_start] = pages_needed;
            search_hint = i + 1;
            lock.unlock();

            auto *header = (HugeHeader *)((char *)region_base + scan_start * PAGE_SIZE);
            header->total_size = total;
            header->is_direct_mmap = false;
            return (char *)header + sizeof(HugeHeader);
          }
        }
      }
    }

    lock.unlock();
    return nullptr;
  }

  // Clear the bitmap bits for this run and release the physical pages back to the OS.
  // The virtual mapping remains so the pages can be reused without another mmap.
  void HugeAllocator::free_page_run(void *ptr, size_t total) {
    size_t offset = (char *)ptr - (char *)region_base;
    size_t start_page = offset / PAGE_SIZE;
    size_t pages = total / PAGE_SIZE;

    lock.lock();

    for (size_t i = start_page; i < start_page + pages; i++) {
      bitmap_clear(alloc_bitmap, i);
    }
    run_lengths[start_page] = 0;

    // Pull search_hint back so future scans can reuse this space.
    if (start_page < search_hint) {
      search_hint = start_page;
    }

    lock.unlock();

    madvise(ptr, total, MADV_DONTNEED);
  }

  // --- Direct mmap ---

  void *HugeAllocator::alloc_direct_mmap(size_t total) {
    void *mem = mmap(nullptr, total, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) return nullptr;

    auto *header = (HugeHeader *)mem;
    header->total_size = total;
    header->is_direct_mmap = true;
    return (char *)header + sizeof(HugeHeader);
  }

  void HugeAllocator::free_direct_mmap(void *ptr, size_t total) {
    munmap(ptr, total);
  }

}  // namespace alaska
