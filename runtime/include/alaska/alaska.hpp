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
        uint64_t misc : 62;  // Some kind of extra info (usually just a pointer)
        unsigned invl : 1;   // This is not a handle
        unsigned swap : 1;   // This handle is swapped
      } alt __attribute__((packed));
    };

    // Encode a handle into the representation used in the
    // top-half of a handle encoding
    ALASKA_INLINE uint64_t encode(void) const {
      auto out = (uint64_t)((uint64_t)this >> handle_squeeze);
      return out;
    }

    // Encode a mapping into a handle that can be later translated by
    // compiler-inserted means.
    ALASKA_INLINE void *to_handle(uint32_t offset = 0) const {
      // The table ensures the m address has bit 32 set. This meaning
      // decoding just checks is a 'is the top bit set?'
      uint64_t out = ((uint64_t)encode() << ALASKA_SIZE_BITS) + offset;
      // printf("encode %p %zu -> %p\n", this, offset, out);
      return (void *)out;
    }

    // Extract an encoded mapping out of the bits of a handle. WARNING: this function does not
    // perform any checking, and will blindly translate any pointer regardless of if it really
    // contains a handle internally.
    static ALASKA_INLINE alaska::Mapping *from_handle(void *handle) {
      return (alaska::Mapping *)((uint64_t)handle >> (ALASKA_SIZE_BITS - handle_squeeze));
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

  // runtime.cpp
  extern void record_translation_info(bool hit);
}  // namespace alaska


// In barrier.c

extern "C" {
// src/lock.c
void *alaska_encode(alaska::Mapping *m, off_t offset);
void *alaska_translate_escape(void *ptr);
void *alaska_translate(void *ptr);
void alaska_release(void *ptr);



// Ensure a handle is present.
void *alaska_ensure_present(alaska::Mapping *m);
}



extern void alaska_dump_backtrace(void);
