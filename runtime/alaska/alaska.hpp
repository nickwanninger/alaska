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
#include <sys/types.h>
#include <alaska/config.h>

#include <alaska/util/utils.h>
#include <alaska/util/list_head.h>
#include <alaska/internal/alaska_internal_malloc.h>
#include <alaska/util/PersistentAllocation.h>
#include <alaska/util/Logger.hpp>

#include <ck/utility.h>
#include <alaska/ftr/ftr.h>



#include <fcntl.h>
#include <unistd.h>
#include <string.h>


#define LTO_INLINE
// #define LTO_INLINE __attribute__((always_inline))


static void show_string(const char *msg) { write(1, msg, strlen(msg)); }




#define alaska_attr_malloc __attribute__((malloc))
#define HANDLE_ADDRSPACE __attribute__((address_space(1)))
#define WORD_ALIGNED(x) (decltype(x))__builtin_assume_aligned(x, sizeof(void *))
// #define WORD_ALIGNED(x) (x)
// check if a pointer is word aligned.
#define IS_WORD_ALIGNED(x) \
  ((((uintptr_t)__builtin_assume_aligned((x), 1)) & (sizeof(void *) - 1)) == 0)
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

#if ALASKA_SIZE_BITS < 32
  using handle_id_t = uint64_t;
#else
  using handle_id_t = uint32_t;
#endif



  class Mapping {
   private:
    union {
      struct {
        uint64_t _value : 62;
        uint64_t pinned : 1;
        uint64_t pending_fault : 1;
      };
      uint64_t value;
    };


   public:
    ALASKA_INLINE void *get_pointer(void) const { return (void *)(uint64_t)this->_value; }

    ALASKA_INLINE void *get_pointer_fast(void) const { return (void *)this->value; }

    inline void invalidate(void) {
#if defined(__riscv) && !defined(ALASKA_YUKON_NO_HARDWARE)
      // Fence *before* the handle invalidation.
      // __asm__ volatile("fence" ::: "memory");
      __asm__ volatile("csrw 0xc4, %0" ::"rK"((uint64_t)handle_id()) : "memory");
#endif
    }

    void set_pointer(void *ptr) {
      // reset();

      this->value = (uint64_t)ptr;
      invalidate();
    }



    // Get the next mapping in the free list. Returns NULL
    // if this isn't a free handle
    alaska::Mapping *get_next(void) {
      if (is_free()) return NULL;
      return (alaska::Mapping *)this->value;
    }

    bool is_free(void) const {
      // A mapping is free if the pointer is NULL, or if the pointer points to a location within the
      // same 2mb page as the mapping itself.

      if (this->value == 0) return true;

      // Check if the pointer is within the same 2mb page as the mapping itself
      uintptr_t mapping_addr = (uintptr_t)this;
      uintptr_t ptr_addr = (uintptr_t)this->value;
      uintptr_t mapping_page = mapping_addr & ~(0x1fffff);  // 2MB page size
      uintptr_t ptr_page = ptr_addr & ~(0x1fffff);          // 2MB page size
      if (mapping_page == ptr_page) {
        // The pointer is within the same 2mb page as the mapping itself
        return true;
      }
      // TODO:
      return false;
    }


    // TODO: should these be atomic?
    bool is_pinned(void) const { return this->pinned; }
    void set_pinned(bool to) { this->pinned = to; }


    bool fault_pending(void) const { return this->pending_fault; }
    void set_fault_pending(bool to) { this->pending_fault = to; }

    void reset(void) {
      this->value = 0;
      invalidate();
    }


    // Encode a handle into the representation used in the
    // top-half of a handle encoding
    ALASKA_INLINE uint64_t encode(void) const {
      auto out = (uint64_t)((uint64_t)this >> ALASKA_SQUEEZE_BITS);
      return out;
      return false;
    }

    ALASKA_INLINE handle_id_t handle_id(void) const {
      uint64_t out = ((uint64_t)encode() << ALASKA_SIZE_BITS);
      return (out & ~(1UL << 63)) >> ALASKA_SIZE_BITS;
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
      auto h = alaska::Mapping::from_handle_safe(handle);
      if (h == nullptr) return handle;
      return h->get_pointer();
    }

    // Extract an encoded mapping out of the bits of a handle. WARNING: this function does not
    // perform any checking, and will blindly translate any pointer regardless of if it really
    // contains a handle internally.
    static ALASKA_INLINE alaska::Mapping *from_handle(void *handle) {
      return WORD_ALIGNED(
          (alaska::Mapping *)((uint64_t)handle >> (ALASKA_SIZE_BITS - ALASKA_SQUEEZE_BITS)));
    }

    static ALASKA_INLINE uint64_t offset_from_handle(void *handle) {
      return (uint64_t)handle & ((1UL << ALASKA_SIZE_BITS) - 1);
    }

    static ALASKA_INLINE bool is_handle_slow(void *ptr) { return ((uint64_t)ptr >> 62) == 0b10; }
    // Extract an encoded mapping out of the bits of a handle. This variant of the function
    // will first check if the pointer provided is a handle. If it is not, this method will
    // return null.
    static ALASKA_INLINE alaska::Mapping *from_handle_safe(void *ptr) {
      if (alaska::Mapping::is_handle_slow(ptr)) {
        return (alaska::Mapping *)((uint64_t)ptr >> (ALASKA_SIZE_BITS - ALASKA_SQUEEZE_BITS));
      }
      // Return null if the pointer is not really a handle
      return nullptr;
    }

    static void *handle_from_hid(handle_id_t id) {
      uint64_t handle = (1UL << 63) | ((uint64_t)id << ALASKA_SIZE_BITS);
      return (void *)handle;
    }

    static ALASKA_INLINE alaska::Mapping *from_handle_id(handle_id_t id) {
      uint64_t handle = (1UL << 63) | ((uint64_t)id << ALASKA_SIZE_BITS);
      return from_handle((void *)handle);
    }

    // Check if a pointer is a handle or not (is the top bit is set?)
    static ALASKA_INLINE bool is_handle(void *ptr) {
      return (int64_t)ptr < 0;  // This is quicker than shifting and masking :)
    }
  };

  static_assert(sizeof(alaska::Mapping) == 8,
                "Mapping must be 8 bytes to fit in a handle. Please fix this.");

  // runtime.cpp
  extern void record_translation_info(bool hit);



  inline void handle_memcpy(void *dst, void *src, size_t size) {
    void *src_data = alaska::Mapping::translate(src);
    void *dst_data = alaska::Mapping::translate(dst);
    memcpy(dst_data, src_data, size);
  }

  inline void handle_memset(void *dst, int value, size_t size) {
    void *dst_data = alaska::Mapping::translate(dst);
    memset(dst_data, value, size);
  }



  // Construct an array of length `length` with default constructors
  template <typename T>
  T *make_object_array(size_t length) {
    // Allocate raw memory for the object
    auto ptr = (T *)alaska_internal_calloc(length, sizeof(T));

    for (size_t i = 0; i < length; i++) {
      // Use placement new to construct the object in the allocated memory
      ::new (ptr + i) T();
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



  class InternalHeapAllocated {
   public:
    void *operator new(size_t size) { return alaska_internal_malloc(size); }
    void operator delete(void *ptr) { alaska_internal_free(ptr); }
  };

}  // namespace alaska
