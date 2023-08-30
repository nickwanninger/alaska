/*
 * This file is part of the Alaska Handle-Based Memory Management System
 *
 * Copyright (c) 2023, Nick Wanninger <ncw@u.northwestern.edu>
 * Copyright (c) 2023, The Constellation Project
 * All rights reserved.
 *
 * This is free software.  You are permitted to use, redistribute,
 * and modify it as specified in the file "LICENSE".
 */

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "./config.h"

#include <alaska/utils.h>
#include <alaska/list_head.h>


#define HANDLE_ADDRSPACE __attribute__((address_space(1)))

namespace alaska {

#ifdef ALASKA_HANDLE_SQUEEZING
  // Must be log2(sizeof(alaska::Mapping))
  constexpr int handle_squeeze = 3;
#else
  constexpr int handle_squeeze = 0;
#endif

  extern long translation_hits;
  extern long translation_misses;

  // A mapping is *just* a pointer. If that pointer has the high bit set, it is swapped out.
  struct Mapping {
    // Represent the fact that a handle is just a pointer w/ "important bit patterns"
    // as a simple union.
    union {
      void *ptr;  // Raw pointer memory
      struct {
        uint64_t info : 63;  // Some kind of info for the swap system
        unsigned flag : 1;   // the high bit indicates this is swapped out
      } swap __attribute__((packed));
    };

    // Encode a handle into the representation used in the
    // top-half of a handle encoding. This *must* be 32bits.
    ALASKA_INLINE uint32_t encode(void) const {
      auto out = (uint32_t)((uint64_t)this >> handle_squeeze);
      // printf("encode %p to %p\n", this, out);
      return out;
    }

    // Encode a mapping into a handle that can be later translated by
    // compiler-inserted means.
    ALASKA_INLINE void *to_handle(uint32_t offset = 0) const {
      // The table ensures the m address has bit 32 set. This meaning
      // decoding just checks is a 'is the top bit set?'
      uint64_t out = ((uint64_t)encode() << 32) + offset;
      // printf("encode %p %zu -> %p\n", this, offset, out);
      return (void *)out;
    }

    // Extract an encoded mapping out of the bits of a handle. WARNING: this function does not
    // perform any checking, and will blindly translate any pointer regardless of if it really
    // contains a handle internally.
    static ALASKA_INLINE alaska::Mapping *from_handle(void *handle) {
      return (alaska::Mapping *)((uint64_t)handle >> (32 - handle_squeeze));
    }

    // Extract an encoded mapping out of the bits of a handle. This variant of the function
    // will first check if the pointer provided is a handle. If it is not, this method will
    // return null.
    static ALASKA_INLINE alaska::Mapping *from_handle_safe(void *ptr) {
      if (alaska::Mapping::is_handle(ptr)) {
        return alaska::Mapping::from_handle(ptr);
      }
      // Return null if the pointer is not really a handle
      return nullptr;
    }

    // Check if a pointer is a handle or not (is the top bit is set?)
    static ALASKA_INLINE bool is_handle(void *ptr) {
      return (int64_t)ptr < 0;  // This is quicker than shifting and masking :)
    }
  };


  /**
   * LockFrame - inserted by the LockInsertion pass in the compiler, this
   * structure is allocated and maintained on each thread managed by alaska.
   * Internally, it contains a all of the actively translated handles in a
   * function. The handles referenced in these LockFrames cannot be moved
   * currently, but could in the future :)
   */
  struct LockFrame {
    alaska::LockFrame *prev;
    uint64_t count;
    void *locked[];
  };

  // runtime.cpp
  extern void record_translation_info(bool hit);
}  // namespace alaska


// In barrier.c
extern "C" __thread alaska::LockFrame *alaska_lock_root_chain;

extern "C" {
// src/lock.c
void *alaska_encode(alaska::Mapping *m, off_t offset);
void *alaska_translate(void *ptr);
void alaska_release(void *ptr);



// Ensure a handle is present.
void *alaska_ensure_present(alaska::Mapping *m);
}



extern void alaska_dump_backtrace(void);
