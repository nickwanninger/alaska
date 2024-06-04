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
  static constexpr size_t heap_size = 128 * gigabyte;

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


   private:
    struct FreePage {
      FreePage *next;
    };

    // This is the memory backing the heap. It is `alaska::heap_size` bytes long.
    void *heap;
    void *end;   // the end of the heap. If bump == end, we are OOM. make heap_size bigger!
    void *bump;  // the current bump pointer

    ck::mutex lock;

    alaska::PageManager::FreePage *free_list;
  };

  class Heap final {
    // A heap is basically just a wrapper around a page manager.
    PageManager pm;

   public:
  };
}  // namespace alaska