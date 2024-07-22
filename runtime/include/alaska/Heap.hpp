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


#include <alaska/HeapPage.hpp>
#include <alaska/SizedPage.hpp>
#include <alaska/SizeClass.hpp>
#include <alaska/Magazine.hpp>
#include <alaska/HugeObjectAllocator.hpp>
#include <alaska/track.hpp>
#include <ck/vec.h>
#include <stdlib.h>
#include <ck/lock.h>

namespace alaska {
  static constexpr size_t kilobyte = 1024;
  static constexpr size_t megabyte = 1024 * kilobyte;
  static constexpr size_t gigabyte = 1024 * megabyte;

  // For now, the heap is a fixed size, large contiguious
  // block of memory managed by the PageManager. Eventually,
  // we will split it up into different mmap regions, but that's
  // just a problem solved by another layer of allocators.

#ifdef ALASKA_TRACK_VALGRIND
  static constexpr uint64_t heap_size_shift_factor = 33;
#else
  static constexpr uint64_t heap_size_shift_factor = 37;
#endif

  static constexpr size_t heap_size = 1LU << heap_size_shift_factor;



  // The PageManager is responsible for managing the memory backing the heap, and subdividing it
  // into pages which are handed to HeapPage instances. The main interface here is to allocate a
  // page of size alaska::page_size, and allow it to be freed again. Fundamentally, the PageManager
  // is a trivial bump allocator that uses a free-list to manage reuse.
  //
  // The methods behind this structure are currently behind a lock.
  class PageManager final {
   public:
    PageManager();
    ~PageManager();


    void *alloc_page(void);
    void free_page(void *page);
    void *get_start(void) const { return heap; }


    inline uint64_t get_allocated_page_count(void) const { return alloc_count; }

   private:
    struct FreePage {
      FreePage *next;
    };

    // This is the memory backing the heap. It is `alaska::heap_size` bytes long.
    void *heap;
    void *end;   // the end of the heap. If bump == end, we are OOM. make heap_size bigger!
    void *bump;  // the current bump pointer
    uint64_t alloc_count = 0;  // How many pages are currently in use
    ck::mutex lock;            // Just a lock.

    alaska::PageManager::FreePage *free_list;
  };



  // allocate pages to fit `bytes` bytes from the kernel.
  void *mmap_alloc(size_t bytes);
  // free pages allocated by mmap_alloc
  void mmap_free(void *ptr, size_t bytes);

  // how many bits are needed to manage page table lookups.
  static constexpr long pt_bits = heap_size_shift_factor - page_shift_factor;
  static constexpr long pt_levels = 2;  // This is only used for math. The structure is 2 levels.
  static_assert(pt_levels == 2, "We require 2 levels of page table");
  static constexpr long bits_per_pt_level = pt_bits / pt_levels;
  static_assert(
      bits_per_pt_level * pt_levels == pt_bits, "bits_per_pt_level * pt_levels != pt_bits");
  static constexpr long pt_level_mask = (1 << bits_per_pt_level) - 1;

  // The HeapPageTable maps the virtual addresses of pages allocated by the PageManager to their
  // managing HeapPage instances. Internally, it operates very similar to a virtual memory page
  // table (a radix tree).
  //
  // TODO: THREAD SAFETY! THREAD SAFETY! THREAD SAFETY!
  class HeapPageTable {
   public:
    HeapPageTable(void *heap_start);
    ~HeapPageTable(void);
    alaska::HeapPage *get(void *page);            // Get the HeapPage given an aligned address
    alaska::HeapPage *get_unaligned(void *page);  // Get the HeapPage given an unaligned address
    void set(void *page, alaska::HeapPage *heap_page);


   private:
    // A pointer to the start of the heap. This is used to compute the page number in the heap.
    void *heap_start;
    alaska::HeapPage **walk(void *page);
    alaska::HeapPage ***root;
  };


  // The Heap provides a simple interface for allocating and freeing memory. It's main job
  // is to take requests for allocations of a certain size, and to redirect those requests to
  // alaska::HeapPage instances, which manage memory issued by the PageManager. The interesting
  // part of this specific heap is that its HeapPages do not all operate the same way. Some pages
  // are used for allocations of a specific size class, while others can mix and match allocation
  // sizes for manufacturing locality and friends.
  class Heap final {
   public:
    PageManager pm;
    HeapPageTable pt;

    HugeObjectAllocator huge_allocator;

    Heap(void);
    ~Heap(void);

    // Get an unowned sized page given a certain size request.
    // TODO: Allow filtering by fullness?
    alaska::SizedPage *get_sizedpage(size_t size, ThreadCache *owner = nullptr);


    void put_sizedpage(alaska::SizedPage *page);

    // Run a "heap collection" phase. This basically just means
    // walking over the HeapPage instances, collecting statistics and
    // updating datastructures. This will take the lock, so it
    // currently only makes sense calling from a single thread.
    void collect(void);


    // Dump the state of the global heap to some file stream.
    void dump(FILE *stream);

   private:
    // This lock is taken whenever global state in the heap is changed by a thread cache.
    ck::mutex lock;
    alaska::Magazine<alaska::SizedPage> size_classes[alaska::num_size_classes];


    // Huge objects are managed independently of the size classes. They are allocated
    // directly from the kernel and are *not handles*
    ck::mutex huge_lock;
    struct list_head huge_allocations;

    struct HugeHeader {
      struct list_head list;
      size_t size;
    };
  };
}  // namespace alaska
