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
  template <typename T>
  class ShuffleAllocator {
   public:
    constexpr static int growsize = 128;
    ShuffleAllocator(void) = default;
    ShuffleAllocator(void *objects, long object_count) { configure(objects, object_count); }

    T *alloc(void) {
      void *object = free_list.pop();
      if (unlikely(object == nullptr)) return alloc_slow();
      alaska_track_malloc_size(object, sizeof(T), sizof(T), 0);

      // printf("%p: Allocate object %lu\n", this->objects_start,
      //     ((uint64_t)object - (uint64_t)this->objects_start) / sizeof(T));

      return (T *)object;
    }

    // Free objects. NOTE: no checks are made that this pointer is valid!
    inline void release_local(void *ptr) {
      return;
      free_list.free_local(ptr);
      alaska_track_free(ptr, 0);
    }

    inline void release_remote(void *ptr) {
      return;
      free_list.free_remote(ptr);
      alaska_track_free(ptr, 0);
    }

    inline void configure(void *objects, long object_count) {
      this->objects_start = this->bump_next = objects;
      this->objects_end = (void *)((uintptr_t)objects + (object_count * sizeof(T)));
      // Re-construct the free list just in case
      this->free_list = ShardedFreeList();

      static_assert(growsize < 256);
    }

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
      return ((uintptr_t)ob - (uintptr_t)this->objects_start) / sizeof(T);
    }



    void reset_free_list(void) { free_list.reset(); }
    void reset_bump_allocator(void *next) { bump_next = next; }

   private:
    __attribute__((noinline)) T *alloc_slow(void) {
      long extended_count = 0;
      T *start = (T *)bump_next;
      T *end = start + growsize;
      if (end > objects_end) end = (T *)objects_end;
      bump_next = (void *)end;

      // If there ends up being no space, return null.
      if (end - start == 0) return nullptr;

      unsigned long real_size = (end - start);
      if (false and real_size == growsize) {
        uint16_t shuffle_vector[real_size];
        // setup the shuffle vector
        for (unsigned long i = 0; i < real_size; i++)
          shuffle_vector[i] = i;
        // Now shuffle
        for (unsigned long i = real_size - 1; i > 0; i--) {
          int j = rand() % (i + 1);
          auto tmp = shuffle_vector[i];
          shuffle_vector[i] = shuffle_vector[j];
          shuffle_vector[j] = tmp;
        }


        for (unsigned long i = 0; i < real_size; i++) {
          free_list.free_local(start + shuffle_vector[i]);
          extended_count++;
        }
      } else {
        // Loop and push the elements to the free list
        for (T *o = end - 1; o >= start; o--) {
          free_list.free_local(o);
          extended_count++;
        }
      }




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

    void *objects_start;                // The start of the object memory
    void *objects_end;                  // The end of the object memory (exclusive)
    void *bump_next;                    // The next object to be bump allocated.
    alaska::ShardedFreeList free_list;  // A free list for tracking releases
  };




};  // namespace alaska
