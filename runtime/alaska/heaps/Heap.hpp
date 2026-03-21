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
#include <alaska/heaps/track.hpp>
#include "alaska/Configuration.hpp"
#include "alaska/heaps/LocalityPage.hpp"
#include <stdlib.h>
#include <time.h>
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

  static inline uint64_t now_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
  }

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

    struct list_head m_age_list;

    void rotate_out(HeapPage &page) {
      page.time_of_last_use = alaska::now_ms();
      list_del(&page.age_list);
      list_add(&page.age_list, &m_age_list);
    }

    template <typename Fn>
    void for_each_old_page(uint64_t min_age_ms, Fn fn) {
      auto cutoff = alaska::now_ms() - min_age_ms;
      HeapPage *entry;
      list_for_each_entry_reverse(entry, &m_age_list, age_list) {
        if (entry->time_of_last_use > cutoff) break;
        fn(entry);
      }
    }



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


    void sweep(void);

    inline bool contains(void *ptr) {
      auto addr = (uintptr_t)ptr;
      return addr >= (uintptr_t)heap_start && addr < (uintptr_t)heap_end;
    }

    // This REQUIRES that the object is actually in the heap, it does not check.
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

    void *alloc_heap_page();

    // This lock is taken whenever global state in the heap is changed by a thread cache.
    ck::mutex lock;
    void *heap_start;
    void *heap_end;
    void *heap_bump;
    alaska::Magazine<alaska::SizedPage> size_classes[alaska::num_size_classes];
    alaska::Magazine<alaska::LocalityPage> locality_pages;
  };


  inline alaska::HeapPage *Heap::get_page(void *object) {
    HeapPageHeader *h = (HeapPageHeader *)((uintptr_t)object & ~(alaska::page_size - 1));
    return h->owner;
  }

  template <typename T, typename Fn>
  T *Heap::find_or_alloc_page(alaska::Magazine<T> &mag, ThreadCache *owner,
                              size_t /*avail_requirement*/, Fn &&init_fn) {
    FTR_FUNCTION();
    // m_available head is kept as the most-recently-returned page (LIFO via put_page).
    // In the common case the head is unowned and has space; at most one owned page is skipped.
    T *p = mag.pop_available(owner);
    if (p != nullptr) {
      p->set_owner(owner);
      return p;
    }

    FTR_SCOPE("AllocHeapPage");
    // No available page — allocate a new one.
    void *memory = alloc_heap_page();
    p = new T(memory);
    mag.add(p);
    p->set_owner(owner);
    init_fn(p);

    return p;
  }
}  // namespace alaska
