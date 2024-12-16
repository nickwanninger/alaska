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
    ~SizedAllocator(void) {
      if (bitmap) alaska_internal_free(bitmap);
    }

    // Allocate an object of size `this->object_size`
    void *alloc(void);

    // Free objects. NOTE: no checks are made that this pointer is valid!
    inline void release_local(void *ptr) {
      free_list.free_local(ptr);
      alaska_track_free(ptr, 0);
      track_freed(ptr);
    }
    inline void release_remote(void *ptr) {
      free_list.free_remote(ptr);
      alaska_track_free(ptr, 0);
      track_freed(ptr);
    }


    void configure(void *objects, size_t object_size, long object_count);

    inline bool some_available(void) {
      return free_list.has_local_free() || free_list.has_remote_free() ||
             (bump_next != objects_end);
    }


    inline long num_free(void) const {
      return num_free_in_free_list() + num_free_in_bump_allocator();
    }

    inline long num_free_in_free_list(void) const { return free_list.num_free(); }

    inline long num_free_in_bump_allocator(void) const {
      return (((uintptr_t)objects_end - (uintptr_t)bump_next) / object_size);
    }


    // return the index of the object
    inline long object_index(void *ob) {
      return ((uintptr_t)ob - (uintptr_t)this->objects_start) / this->object_size;
    }

    long extend(long count);


    void reset_free_list(void) { free_list.reset(); }
    void reset_bump_allocator(void *next) { bump_next = next; }

    // An internal-ish number which represents the extent of objects
    // which have been bump allocated in this allocator so far.
    inline long object_extent(void) {
      return ((off_t)bump_next - (off_t)objects_start) / object_size;
    }


    class AllocatedObjectIterator {
     public:
      // Constructor
      AllocatedObjectIterator(
          const uint8_t *bitmap, size_t size_in_bits, void *start, size_t object_size)
          : bitmap(bitmap)
          , size(size_in_bits)
          , current_index(0)
          , start_object(start)
          , object_size(object_size) {
        advance_to_next_set_bit();
      }
      AllocatedObjectIterator(void) : current_index(-1) {

      }



      // Iterator methods
      bool operator!=(const AllocatedObjectIterator &other) const {
        return current_index != other.current_index;
      }

      void *operator*() const {
        return (void *)((off_t)start_object + current_index * object_size);
      }

      AllocatedObjectIterator &operator++() {
        ++current_index;
        advance_to_next_set_bit();
        return *this;
      }

     private:
      const uint8_t *bitmap;
      long size;
      long current_index;

      void *start_object;
      size_t object_size;

      void advance_to_next_set_bit() {
        while (current_index < size) {
          size_t byte_index = current_index / 8;
          size_t bit_offset = current_index % 8;

          if (bitmap[byte_index] & (1 << bit_offset)) {
            return;
          }
          ++current_index;
        }

        // Mark as end
        current_index = -1;
      }
    };

    AllocatedObjectIterator begin() const {
      return AllocatedObjectIterator(bitmap, object_count, objects_start, object_size);
    }
    AllocatedObjectIterator end() const { return AllocatedObjectIterator(); }


   private:
    inline bool is_allocated(void *p) {
      auto i = object_index(p);
      uint8_t byte_value = __atomic_load_n(&bitmap[i / 8], __ATOMIC_RELAXED);
      return (byte_value & (1 << (i % 8))) != 0;
    }
    inline void track_allocated(void *p) {
      auto i = object_index(p);
      __atomic_fetch_or(&bitmap[i / 8], 1 << (i % 8), __ATOMIC_RELAXED);
    }

    inline void track_freed(void *p) {
      auto i = object_index(p);
      __atmic_fetch_and(&bitmap[i / 8], ~(1 << (i % 8)), __ATOMIC_RELAXED);
    }

    void *alloc_slow(void);

    size_t object_count;                // How many objects there are in this heap
    void *objects_start;                // The start of the object memory
    void *objects_end;                  // The end of the object memory (exclusive)
    void *bump_next;                    // The next object to be bump allocated.
    size_t object_size;                 // How large each object is
    uint8_t *bitmap = nullptr;          // A pointer to the start of the free bitmap
    alaska::ShardedFreeList free_list;  // A free list for tracking releases
  };




  __attribute__((always_inline)) inline void *SizedAllocator::alloc(void) {
    void *object = free_list.pop();

    if (unlikely(object == nullptr)) {
      return alloc_slow();
    }

    alaska_track_malloc_size(object, object_size, object_size, 0);
    track_allocated(object);

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
    long extended_count = 0;
    off_t start = (off_t)bump_next;
    off_t end = start + object_size * count;
    if (end > (off_t)objects_end) end = (off_t)objects_end;
    bump_next = (void *)end;

    for (off_t o = end - object_size; o >= start; o -= object_size) {
      free_list.free_local((void *)o);
      extended_count++;
    }

    return extended_count;
  }


  inline void SizedAllocator::configure(void *objects, size_t object_size, long object_count) {
    this->objects_start = this->bump_next = objects;
    // the number of objects in this array
    this->object_count = object_count;
    this->objects_end = (void *)((uintptr_t)objects + (object_count * object_size));
    this->object_size = object_size;


    if (bitmap) {
      alaska_internal_free(bitmap);
      bitmap = nullptr;
    }
    // Allocate the bitmap to track which objects are allocated.
    auto bitmap_size = (object_count + 8 - 1) / 8;
    this->bitmap = (uint8_t *)alaska_internal_calloc(1, bitmap_size);
    // Re-construct the free list just in case
    this->free_list = ShardedFreeList();
  }

};  // namespace alaska
