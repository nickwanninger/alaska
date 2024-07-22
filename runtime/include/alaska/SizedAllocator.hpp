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

#include <stdlib.h>
#include <sys/cdefs.h>
#include <alaska/ShardedFreeList.hpp>
#include <alaska/HeapPage.hpp>
#include <alaska/track.hpp>

namespace alaska {



  // A general abstraction for allocating objects from a block of memory
  // NOTE: it is assumed that objects_end is aligned to object_size somehow.
  class SizedAllocator {
   public:
    SizedAllocator(void) = default;
    SizedAllocator(void *objects, size_t object_size, long object_count) {
      configure(objects, object_size, object_count);
    }

    // Allocate an object of size `this->object_size`
    void *alloc(void);

    // Free objects. NOTE: no checks are made that this pointer is valid!
    inline void release_local(void *ptr) {
      free_list.free_local(ptr);
      alaska_track_free(ptr, 0);
    }
    inline void release_remote(void *ptr) {
      free_list.free_remote(ptr);
      alaska_track_free(ptr, 0);
    }

    void configure(void *objects, size_t object_size, long object_count);

    // return the index of the object
    inline long object_index(void *ob) {
      return ((uintptr_t)ob - (uintptr_t)this->objects_start) / this->object_size;
    }

   private:
    void *alloc_slow(void);
    long extend(long count);

    void *objects_start;                // The start of the object memory
    void *objects_end;                  // The end of the object memory (exclusive)
    void *bump_next;                    // The next object to be bump allocated.
    size_t object_size;                 // How large each object is
    alaska::ShardedFreeList free_list;  // A free list for tracking releases
  };




  inline void *SizedAllocator::alloc(void) {
    void *object = free_list.pop();

    if (unlikely(object == nullptr)) {
      return alloc_slow();
    }

    alaska_track_malloc_size(object, object_size, object_size, 0);

    return object;
  }


  __attribute__((noinline)) inline void *SizedAllocator::alloc_slow(void) {
    long extended_count = extend(128);

    // 1. If we managed to extend the list, return one of the blocks from it.
    if (extended_count > 0) {
      // Fall back into the alloc function to do the heavy lifting of actually allocating
      // one of the blocks we just extended the list with.
      return alloc();
    }

    // 2. If the list was not extended, try swapping the remote_free list and the local_free list.
    // This is a little tricky because we need to worry about atomics here.
    free_list.swap();

    // If local-free is still null, return null
    if (not free_list.has_local_free()) return nullptr;

    // Otherwise, fall back to alloc
    return alloc();
  }


  inline long SizedAllocator::extend(long count) {
    long e = 0;
    for (; e < count && bump_next != objects_end; e++) {
      free_list.free_local(bump_next);
      bump_next = (void *)((uintptr_t)bump_next + object_size);
    }
    return e;
  }


  inline void SizedAllocator::configure(void *objects, size_t object_size, long object_count) {
    this->objects_start = this->bump_next = objects;
    this->objects_end = (void *)((uintptr_t)objects + (object_count * object_size));
    this->object_size = object_size;
    // Re-construct the free list just in case
    this->free_list = ShardedFreeList();
  }


};  // namespace alaska
