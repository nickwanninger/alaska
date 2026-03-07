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

#include <alaska/util/utils.h>
#include <stdio.h>

namespace alaska {

  // This structure abstracts the concept of a block in the free list.
  // This is needed so
  struct DefaultFreeListBlock {
    DefaultFreeListBlock *next;

    inline void setNext(DefaultFreeListBlock *n) { next = n; }
    inline DefaultFreeListBlock *getNext(void) const { return next; }

    inline void markFreed(void) {}
    inline void markAllocated(void) {}
  };

  // This class provides a singular abstraction for managing local/remote free lists of simple
  // block-like objects in memory. It works entirely using `void*` pointers to blocks of memory,
  // and doesn't really care what the memory is, so long as it is at least 8 bytes big.
  template <typename Block = DefaultFreeListBlock>
  class ShardedFreeList final {
   private:
    Block *local_free = nullptr;
    Block *remote_free = nullptr;

   public:
    inline Block *peek(void) const { return local_free; }
    // Pop from the local free list. Return null if the local free list is empty
    inline Block *pop(void) { return pop(local_free); }
    inline Block *pop(Block *b) {
      if (unlikely(b != nullptr)) {
        local_free = local_free->next;
      }
      b->markAllocated();
      return b;
    }

    inline Block *pop_unchecked(Block *b) {
      local_free = b->next;
      b->markAllocated();
      return b;
    }


    inline bool has_local_free(void) const { return local_free != nullptr; }
    // WEAK ORDERING!
    inline bool has_remote_free(void) const { return remote_free != nullptr; }

    inline bool has_any_free(void) const { return has_local_free() || has_remote_free(); }


    // Ask the free list to swap remote_free into the local_free list atomically.
    inline void swap(void) {
      if (local_free != nullptr) return;  // Sanity!
      this->local_free = __atomic_exchange_n(&this->remote_free, nullptr, __ATOMIC_ACQUIRE);
    }


    inline void free_local(void *p) {
      auto *b = (Block *)p;
      b->next = local_free;
      b->markFreed();
      local_free = b;
    }

    // Thread-safe version of free_local using atomics.
    // Can be safely called from multiple threads.
    __attribute__((noinline)) inline void free_local_atomic(void *p) {
      auto *block = (Block *)p;
      Block **list = &local_free;
      // Use atomic CAS to safely push onto the local free list
      do {
        block->next = *list;
      } while (!__atomic_compare_exchange_n(list, &block->next, block, 1, __ATOMIC_RELEASE,
                                            __ATOMIC_RELAXED));

      block->markFreed();
    }

    __attribute__((noinline)) inline void free_remote(void *p) {
      auto *block = (Block *)p;
      Block **list = &remote_free;

      do {
        block->next = *list;
      } while (!__atomic_compare_exchange_n(list, &block->next, block, 1, __ATOMIC_RELEASE,
                                            __ATOMIC_RELAXED));

      block->markFreed();
    }

    // Wipe out the free lists.
    void reset() {
      local_free = remote_free = NULL;
    }
  };




}  // namespace alaska
