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

  template <typename T>
  class ObjectAllocator {
   public:
    ObjectAllocator(void) = default;
    ObjectAllocator(void *objects, long object_count) { configure(objects, object_count); }

    T *alloc(void);

    // Free objects. NOTE: no checks are made that this pointer is valid!
    inline void release_local(void *ptr) {
      free_list.free_local(ptr);
      alaska_track_free(ptr, 0);
    }
    inline void release_remote(void *ptr) {
      free_list.free_remote(ptr);
      alaska_track_free(ptr, 0);
    }

    void configure(void *objects, long object_count);

    inline bool some_available(void) {
      return free_list.has_local_free() || free_list.has_remote_free() ||
             (bump_next != objects_end);
    }


    inline long num_free(void) const {
      return num_free_in_free_list() + num_free_in_bump_allocator();
    }

    inline long num_free_in_free_list(void) const { return free_list.num_free(); }

    inline long num_free_in_bump_allocator(void) const {
      return (((uintptr_t)objects_end - (uintptr_t)bump_next) / sizeof(T));
    }


    // return the index of the object
    inline long object_index(void *ob) {
      return ((uintptr_t)ob - (uintptr_t)this->objects_start) / this->object_size;
    }

    long extend(long count);

   private:
    T *alloc_slow(void);

    void *objects_start;                // The start of the object memory
    void *objects_end;                  // The end of the object memory (exclusive)
    void *bump_next;                    // The next object to be bump allocated.
    alaska::ShardedFreeList free_list;  // A free list for tracking releases
  };




  template <typename T>
  __attribute__((always_inline)) inline T *ObjectAllocator<T>::alloc(void) {
    void *object = free_list.pop();

    if (unlikely(object == nullptr)) {
      return alloc_slow();
    }

    alaska_track_malloc_size(object, sizeof(T), object_size, 0);

    return (T*)object;
  }


  template <typename T>
  __attribute__((noinline)) inline T *ObjectAllocator<T>::alloc_slow(void) {
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


  template <typename T>
  inline long ObjectAllocator<T>::extend(long count) {
    long e = 0;
    for (; e < count && bump_next != objects_end; e++) {
      free_list.free_local(bump_next);
      bump_next = (void *)((uintptr_t)bump_next + sizeof(T));
    }
    return e;
  }


  template <typename T>
  inline void ObjectAllocator<T>::configure(void *objects, long object_count) {
    this->objects_start = this->bump_next = objects;
    this->objects_end = (void *)((uintptr_t)objects + (object_count * sizeof(T)));
    // Re-construct the free list just in case
    this->free_list = ShardedFreeList();
  }

};  // namespace alaska
