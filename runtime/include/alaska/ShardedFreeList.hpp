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

#include <alaska/utils.h>
#include <stdio.h>

namespace alaska {


  // This class provides a singular abstraction for managing local/remote free lists of simple
  // block-like objects in memory. It works entirely using `void*` pointers to blocks of memory,
  // and doesn't really care what the memory is, so long as it is at least 8 bytes big.
  class ShardedFreeList final {
   public:
    // Pop from the local free list. Return null if the local free list is empty
    void *pop(void);
    inline bool has_local_free(void) const { return local_free != nullptr; }
    // WEAK ORDERING!
    inline bool has_remote_free(void) const { return remote_free != nullptr; }


    inline long num_free(void) const {
      // atomics?
      return num_local_free + num_remote_free;
    }

    // Ask the free list to swap remote_free into the local_free list atomically.
    void swap(void);


    void free_local(void *);
    void free_remote(void *);

    // Wipe out the free lists.
    void reset() {
      local_free = remote_free = NULL;
      num_local_free = num_remote_free = 0;
    }



   private:
    template <typename T>
    inline static T *add_invalid_bit(T *p) {
      return (T *)((uint64_t)p | 1LU);
    }

    template <typename T>
    inline static T *remove_invalid_bit(T *ptr) {
      return (T *)((uint64_t)ptr & ~1LU);
    }

    // A simple `Block` structure to give us nicer linked-list style access
    class Block {
     public:
      Block *encoded_next;

      void set_next(Block *next) { this->encoded_next = add_invalid_bit(next); }
      Block *get_next(void) { return remove_invalid_bit(encoded_next); }
    };

    Block *local_free = nullptr;
    Block *remote_free = nullptr;


    long num_local_free = 0;
    long num_remote_free = 0;
  };


  ////////////////////////////////////////////////////////////////////////////////////////////

  __attribute__((always_inline))  // We want this to be inlined wherever it can be
  inline void *
  ShardedFreeList::pop(void) {
    void *b = (void *)local_free;
    if (unlikely(b != nullptr)) {
      num_local_free--;
      local_free = local_free->get_next();
    }

    return b;
  }



  __attribute__((always_inline))  // We want this to be inlined wherever it can be
  inline void
  ShardedFreeList::swap(void) {
    if (local_free != nullptr) return;  // Sanity!
    do {
      this->local_free = this->remote_free;
    } while (!__atomic_compare_exchange_n(
        &this->remote_free, &this->local_free, nullptr, 1, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED));

    // ??? ATOMICS ???
    num_local_free += num_remote_free;
    atomic_set(num_remote_free, 0);
  }




  __attribute__((always_inline))  // We want this to be inlined wherever it can be
  inline void
  ShardedFreeList::free_local(void *p) {
    auto *b = (Block *)p;
    b->set_next(local_free);
    local_free = b;
    num_local_free++;
  }


  __attribute__((always_inline))  // We want this to be inlined wherever it can be
  inline void
  ShardedFreeList::free_remote(void *p) {
    auto *block = (Block *)p;

    Block *invalid_remote_free = add_invalid_bit(remote_free);
    Block **list = &invalid_remote_free;
    // TODO: NOT SURE ABOUT THE CONSISTENCY OPTIONS HERE
    do {
      block->set_next(*list);
    } while (!__atomic_compare_exchange_n(
        list, &block->encoded_next, add_invalid_bit(block), 1, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED));

    // num_remote_free ++;
    atomic_inc(num_remote_free, 1);
  }



}  // namespace alaska
