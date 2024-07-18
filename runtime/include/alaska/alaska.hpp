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
#include <alaska/utils.h>
#include <alaska/list_head.h>
#include <alaska/autoconf.h>
#include <alaska/liballoc.h>

#include <ck/utility.h>

#define HANDLE_ADDRSPACE __attribute__((address_space(1)))

// Fwd decl stuff
namespace alaska {
  class Mapping;
}

extern "C" {
// src/translate.cpp
void *alaska_encode(alaska::Mapping *m, off_t offset);
void *alaska_translate_escape(void *ptr);
void *alaska_translate(void *ptr);
void alaska_release(void *ptr);
void *alaska_ensure_present(alaska::Mapping *m);
}

namespace alaska {

  extern long translation_hits;
  extern long translation_misses;


  class Mapping {
   private:
    // Represent the fact that a handle is just a pointer w/ "important bit patterns"
    // as a simple union.
    union {
      void *ptr;  // Raw pointer memory
      struct {
        uint64_t misc : 61;   // Some kind of extra info (usually just a pointer)
        unsigned pinned : 1;  // If this handle is pinned currently.
        unsigned invl : 1;    // This handle is not mapped. ptr is a free list
        unsigned swap : 1;    // This handle is swapped
      } alt __attribute__((packed));
    };

   public:
    // Return the pointer. If it is free, return NULL
    ALASKA_INLINE void *get_pointer(void) const {
#ifdef ALASKA_SWAP_SUPPORT
      // If swapping is enabled, the top bit will be set, so we need to check that
      if (unlikely(alt.swap)) {
        // Ask the runtime to "swap" the object back in. We blindly assume that
        // this will succeed for performance reasons.
        return alaska_ensure_present(this);
      }
#endif
      return ptr;
    }

    void set_pointer(void *ptr) {
      reset();
      this->ptr = ptr;
      alt.invl = 0;
    }


    // Get the next mapping in the free list. Returns NULL
    // if this isn't a free handle
    alaska::Mapping *get_next(void) {
      if (is_free()) return NULL;
      return (alaska::Mapping *)(uint64_t)alt.misc;
    }

    void set_next(alaska::Mapping *next) {
      reset();
      alt.misc = (uint64_t)next;
      alt.invl = 1;
    }


    bool is_free(void) const { return alt.invl; }


    // TODO: should these be atomic?
    bool is_pinned(void) const { return this->alt.pinned; }
    void set_pinned(bool to) { this->alt.pinned = to; }


    void reset(void) {
      ptr = NULL;
      alt.invl = 0;
      alt.swap = 0;
    }


    // Encode a handle into the representation used in the
    // top-half of a handle encoding
    ALASKA_INLINE uint64_t encode(void) const {
      auto out = (uint64_t)((uint64_t)this >> ALASKA_SQUEEZE_BITS);
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

    static void *translate(void *handle) {
      return alaska::Mapping::from_handle(handle)->get_pointer();
    }

    // Extract an encoded mapping out of the bits of a handle. WARNING: this function does not
    // perform any checking, and will blindly translate any pointer regardless of if it really
    // contains a handle internally.
    static ALASKA_INLINE alaska::Mapping *from_handle(void *handle) {
      return (alaska::Mapping *)((uint64_t)handle >> (ALASKA_SIZE_BITS - ALASKA_SQUEEZE_BITS));
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




  template <typename T, typename... Args>
  T *make_object(Args &&...args) {
    // Allocate raw memory for the object
    void *ptr = alaska_internal_malloc(sizeof(T));
    // Use placement new to construct the object in the allocated memory
    new (ptr) T(args...);
    return (T *)ptr;
  }

  template <typename T>
  void delete_object(T *ptr) {
    if (ptr) {
      // Call the destructor explicitly
      ptr->~T();
      // Free the raw memory
      alaska_internal_free(ptr);
    }
  }


  // Construct an array of length `length` with default constructors
  template <typename T>
  T *make_object_array(size_t length) {
    // Allocate raw memory for the object
    auto ptr = (T *)alaska_internal_calloc(length, sizeof(T));

    for (size_t i = 0; i < length; i++) {
      // Use placement new to construct the object in the allocated memory
      new (ptr + i) T();
    }
    return ptr;
  }
  // Construct an array of length `length` with default constructors
  template <typename T>
  void delete_object_array(T *array, size_t length) {
    for (size_t i = 0; i < length; i++) {
      // call dtor
      array[i].~T();
    }
    alaska_internal_free((void *)array);
  }


}  // namespace alaska
