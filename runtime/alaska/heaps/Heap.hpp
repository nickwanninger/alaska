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

#include <alaska/handles/ObjectHeader.hpp>
#include <alaska/heaps/HeapPage.hpp>
#include <alaska/heaps/SizedPage.hpp>
#include <alaska/heaps/SizeClass.hpp>
#include <alaska/heaps/Magazine.hpp>
#include <alaska/heaps/HugeObjectAllocator.hpp>
#include <alaska/heaps/track.hpp>
#include "alaska/Configuration.hpp"
#include "alaska/heaps/LocalityPage.hpp"
#include <ck/vec.h>
#include <stdlib.h>
#include <ck/lock.h>

namespace alaska {
  static constexpr size_t kilobyte = 1024;
  static constexpr size_t megabyte = 1024 * kilobyte;
  static constexpr size_t gigabyte = 1024 * megabyte;


  // For now, the heap is a fixed size, large contiguous
  // block of memory reserved from the operating system. Eventually,
  // we will split it up into different mmap regions, but that's
  // just a problem solved by another layer of allocators.


#ifndef HEAP_SIZE_SHIFT_FACTOR
#define HEAP_SIZE_SHIFT_FACTOR 35
#endif
#ifdef ALASKA_TRACK_VALGRIND
  static constexpr uint64_t heap_size_shift_factor = 33;
#else
  static constexpr uint64_t heap_size_shift_factor = HEAP_SIZE_SHIFT_FACTOR;
#endif

  static constexpr size_t default_heap_size = 1LU << heap_size_shift_factor;



  // allocate pages to fit `bytes` bytes from the kernel.
  void *mmap_alloc(size_t bytes);
  // free pages allocated by mmap_alloc
  void mmap_free(void *ptr, size_t bytes);

  class Heap final {
   public:
    Heap(alaska::Configuration &config);
    ~Heap(void);

    // Get an unowned sized page given a certain size request.
    // TODO: Allow filtering by fullness?
    alaska::SizedPage *get_sizedpage(size_t size, ThreadCache *owner = nullptr);
    alaska::LocalityPage *get_localitypage(size_t size_requirement, ThreadCache *owner = nullptr);


    void put_page(alaska::SizedPage *page);
    void put_page(alaska::LocalityPage *page);



    // Run a "heap collection" phase. This basically just means
    // walking over the HeapPage instances, collecting statistics and
    // updating datastructures. This will take the lock, so it
    // currently only makes sense calling from a single thread.
    void collect(void);


    // Dump the state of the global heap to some file stream.
    void dump(FILE *stream);
    void dump_html(FILE *stream);
    void dump_json(FILE *stream);

    // Run a compaction on sized pages.
    long compact_sizedpages(void);
    long compact_locality_pages(void);

    inline bool contains(void *ptr) {
      auto addr = (uintptr_t)ptr;
      return addr >= (uintptr_t)heap_start && addr < (uintptr_t)heap_end;
    }

    const ck::vec<alaska::HeapPage *> &get_page_table(void) const { return page_table; }


    // get the page containing some object.
    static alaska::HeapPage *get_page(void *object);

    template <typename Fn>
    void for_each_page(Fn fn) {
      for (size_t i = 0; i < alaska::num_size_classes; i++) {
        size_classes[i].for_each([=](auto *p) {
          fn((alaska::HeapPage *)p);
          return true;
        });
      }
      locality_pages.for_each([=](auto *p) {
        fn((alaska::HeapPage *)p);
        return true;
      });
    }

    void collect(ThreadCache *tc, int sc);


   private:
    size_t heap_size;

    template <typename T, typename Fn>
    T *find_or_alloc_page(alaska::Magazine<T> &mag, ThreadCache *owner, size_t avail_requirement,
                          Fn &&init);

    void register_page(void *page, alaska::HeapPage *hp);
    alaska::HeapPage **walk_page_table(void *page, bool ensure = false);
    void *alloc_heap_page();

    // This lock is taken whenever global state in the heap is changed by a thread cache.
    ck::mutex lock;
    void *heap_start;
    void *heap_end;
    void *heap_bump;
    ck::vec<alaska::HeapPage *> page_table;
    alaska::Magazine<alaska::SizedPage> size_classes[alaska::num_size_classes];
    alaska::Magazine<alaska::LocalityPage> locality_pages;
  };


  inline alaska::HeapPage *Heap::get_page(void *object) {
    HeapPageHeader *h = (HeapPageHeader *)((uintptr_t)object & ~(alaska::page_size - 1));
    if (h->magic != HeapPageHeader::expected_magic) return nullptr;
    return h->owner;
  }

  inline void Heap::register_page(void *page, alaska::HeapPage *hp) {
    auto **entry = walk_page_table(page, true);
    *entry = hp;
  }

  inline alaska::HeapPage **Heap::walk_page_table(void *vpage, bool ensure) {
    uintptr_t page_off = (uintptr_t)vpage - (uintptr_t)heap_start;
    off_t page_number = page_off >> alaska::page_shift_factor;

    if (page_number >= page_table.size()) {
      if (!ensure) {
        return nullptr;
      }
      page_table.resize(page_number + 1);
    }

    return &page_table[page_number];
  }


  template <typename T, typename Fn>
  T *Heap::find_or_alloc_page(alaska::Magazine<T> &mag, ThreadCache *owner,
                              size_t avail_requirement, Fn &&init_fn) {
    long num_full = 0;
    long num_found = 0;
    if (mag.size() != 0) {
      T *best = nullptr;
      // alaska::printf("Searching for page with at least %zu available\n", avail_requirement);
      mag.for_each([&](T *p) {
        size_t avail = p->available();
        if (avail == 0) num_full++;

        num_found++;
        if (avail >= avail_requirement && p->get_owner() == nullptr) {
          best = p;
          return false;
        }

        return true;



        // if (avail == 0) {
        //   num_full++;
        //   return true;
        // } else {
        //   best = p;
        //   return false;
        // }

        if ((size_t)avail >= (size_t)avail_requirement and p->get_owner() == nullptr) {
          // best = p;
          if (best == nullptr) {
            best = p;
            return true;
          }


          if ((long)avail >= (long)best->available()) {
            best = p;
          } else {
            // .. Nothing
          }
        }
        return true;
      });

      // printf("Find or alloc page. looked at %8ld pages, %8ld full (%.2f%%)\n", num_found,
      // num_full,
      //     (float)num_full / (float)num_found * 100.0f);

      if (best != NULL) {
        best->set_owner(owner);
        return best;
      }
    }


    // Allocate a new sized page
    void *memory = alloc_heap_page();
    T *p = new T(memory);
    // Map it in the page table for fast lookup
    register_page(memory, p);
    mag.add(p);
    p->set_owner(owner);

    init_fn(p);

    return p;
  }
}  // namespace alaska
