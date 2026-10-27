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

  void *HugeAllocator::alloc(size_t size) { return alloc_aligned(size, sizeof(HugeHeader)); }

  void *HugeAllocator::alloc_aligned(size_t size, size_t align) {
    if (size == 0) return nullptr;
    // Stop align_up from wrapping around to a small size.
    if (size > SIZE_MAX / 2 || align > SIZE_MAX / 4) return nullptr;
    if (align < sizeof(HugeHeader)) align = sizeof(HugeHeader);

    // Runs and mappings start on a page, so the data can only start on a larger
    // alignment if the mapping itself is aligned.
    if (align > PAGE_SIZE) return alloc_direct_mmap_aligned(size, align);

    size_t data_offset = align_up(sizeof(HugeHeader), align);
    size_t total = align_up(data_offset + size, PAGE_SIZE);

    void *base = nullptr;
    bool direct = total >= DIRECT_MMAP_THRESH || region_base == nullptr;
    if (!direct) {
      base = alloc_page_run(total);
      direct = base == nullptr;
    }
    if (direct) base = alloc_direct_mmap(total);
    if (base == nullptr) return nullptr;

    return place(base, total, data_offset, direct);
  }

  // Write the header for a run or mapping at `base` and return the caller's pointer.
  void *HugeAllocator::place(void *base, size_t total, size_t data_offset, bool direct) {
    void *ptr = (char *)base + data_offset;
    auto *header = (HugeHeader *)((char *)ptr - sizeof(HugeHeader));
    header->total_size = total;
    header->data_offset = data_offset;

    if (direct) {
      lock.lock();
      direct_maps.add((uintptr_t)ptr);
      lock.unlock();
    }
    return ptr;
  }

  bool HugeAllocator::free(void *ptr) {
    if (ptr == nullptr) return true;

    if (in_region(ptr)) {
      auto *header = (HugeHeader *)((char *)ptr - sizeof(HugeHeader));
      free_page_run((char *)ptr - header->data_offset, header->total_size);
      return true;
    }

    // Only read the header after the ownership check: a foreign pointer has none.
    lock.lock();
    bool found = direct_maps.contains((uintptr_t)ptr);
    if (found) direct_maps.remove((uintptr_t)ptr);
    lock.unlock();

    if (!found) return false;
    auto *header = (HugeHeader *)((char *)ptr - sizeof(HugeHeader));
    free_direct_mmap((char *)ptr - header->data_offset, header->total_size);
    return true;
  }

  size_t HugeAllocator::usable_size(void *ptr) {
    if (ptr == nullptr) return 0;
    auto *header = (HugeHeader *)((char *)ptr - sizeof(HugeHeader));
    return header->total_size - header->data_offset;
  }

  bool HugeAllocator::in_region(void *ptr) {
    auto addr = (uintptr_t)ptr;
    auto base = (uintptr_t)region_base;
    return region_base != nullptr && addr >= base && addr < base + region_size;
  }

  bool HugeAllocator::owns(void *ptr) {
    if (ptr == nullptr) return false;
    if (in_region(ptr)) return true;

    lock.lock();
    bool found = direct_maps.contains((uintptr_t)ptr);
    lock.unlock();
    return found;
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
  // return the start of the run. Starts from search_hint and wraps around.
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

          return (char *)region_base + scan_start * PAGE_SIZE;
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

            return (char *)region_base + scan_start * PAGE_SIZE;
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

    return mem;
  }

  // Map more than needed, then unmap the pages before and after the aligned part.
  // The run starts one page before the caller's pointer, so the header fits.
  void *HugeAllocator::alloc_direct_mmap_aligned(size_t size, size_t align) {
    size_t length = align_up(size, PAGE_SIZE) + PAGE_SIZE + align;
    void *mem = mmap(nullptr, length, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) return nullptr;

    uintptr_t start = (uintptr_t)mem;
    uintptr_t ptr = align_up(start + PAGE_SIZE, align);
    uintptr_t base = ptr - PAGE_SIZE;
    uintptr_t end = ptr + align_up(size, PAGE_SIZE);

    if (base > start) munmap(mem, base - start);
    if (start + length > end) munmap((void *)end, start + length - end);

    return place((void *)base, end - base, PAGE_SIZE, true);
  }

  void HugeAllocator::free_direct_mmap(void *ptr, size_t total) {
    munmap(ptr, total);
  }

}  // namespace alaska
