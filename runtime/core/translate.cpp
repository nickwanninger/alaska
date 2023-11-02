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
#include <assert.h>
#include <errno.h>
#include <execinfo.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include <alaska.h>
#include <dlfcn.h>
#include <alaska/alaska.hpp>
#include <alaska/config.h>
#include <alaska/utils.h>


/**
 * Note: This file is inlined by the compiler to make locks faster.
 * Do not declare any global variables here, as they may get overwritten
 * or duplciated needlessly. (Which can lead to linker errors)
 */

extern int alaska_verify_is_locally_locked(void *ptr);

static ALASKA_INLINE void alaska_track_hit(void) {
#ifdef ALASKA_TRACK_TRANSLATION_HITRATE
  alaska::record_translation_info(true);
  // alaska::translation_hits++;
#endif
}

static ALASKA_INLINE void alaska_track_miss(void) {
#ifdef ALASKA_TRACK_TRANSLATION_HITRATE
  alaska::record_translation_info(false);
  // alaska::translation_misses++;
#endif
}


extern "C" {
extern int __LLVM_StackMaps __attribute__((weak));
}


extern "C" void *alaska_translate_uncond(void *ptr) {
  int64_t bits = (int64_t)ptr;

  auto m = alaska::Mapping::from_handle(ptr);
  // Pull the address from the mapping
  void *mapped = m->ptr;
#ifdef ALASKA_SWAP_SUPPORT
  // If swapping is enabled, the top bit will be set, so we need to check that
  if (unlikely(m->alt.swap)) {
    // Ask the runtime to "swap" the object back in. We blindly assume that
    // this will succeed for performance reasons.
    mapped = alaska_ensure_present(m);
  }
#endif
  // ALASKA_SANITY(mapped != NULL, "Mapped pointer is null for handle %p\n", ptr);
  // Apply the offset to the mapping and return it.
  ptr = (void *)((uint64_t)mapped + (uint32_t)bits);
  return ptr;
}

void *alaska_translate(void *ptr) {
  int64_t bits = (int64_t)ptr;

  // If the pointer is "greater than zero", then it is not a handle. This is because we rely on the
  // fact that most architectures have a "branch if less than 0" instruction to detect this. Most
  // arch just check the top bit to implement that, which is all we need!
  if (unlikely(bits >= 0)) {
    alaska_track_miss();
    return ptr;
  }

  alaska_track_hit();

  // Grab the mapping from the runtime
  auto m = alaska::Mapping::from_handle(ptr);
  // Pull the address from the mapping
  void *mapped = m->ptr;
#ifdef ALASKA_SWAP_SUPPORT
  // If swapping is enabled, the top bit will be set, so we need to check that
  if (unlikely(m->alt.swap)) {
    // Ask the runtime to "swap" the object back in. We blindly assume that
    // this will succeed for performance reasons.
    mapped = alaska_ensure_present(m);
  }
#endif
  // ALASKA_SANITY(mapped != NULL, "Mapped pointer is null for handle %p\n", ptr);
  // Apply the offset to the mapping and return it.
  ptr = (void *)((uint64_t)mapped + (uint32_t)bits);
  return ptr;
}




void alaska_release(void *ptr) {
  // This function is just a marker that `ptr` is now dead (no longer used)
  // and should not have any real meaning in the runtime
}

uint64_t base_start = 0;

void print_backtrace() {
  void *rbp = (void *)__builtin_frame_address(0);
  uint64_t start, end;
  start = (uint64_t)rbp;
  while ((uint64_t)rbp > 0x1000) {
    end = (uint64_t)rbp;
    if (end > base_start) base_start = end;
    rbp = *(void **)rbp;  // Follow the chain of rbp values
  }
  printf(" (%zd)\n", end - start);
}

extern bool alaska_should_safepoint;
extern "C" __attribute__((preserve_all)) uint64_t alaska_barrier_poll();
extern "C" void alaska_show_backtrace(void);


extern "C" void alaska_safepoint(void) {
  // *(volatile int *)ALASKA_SAFEPOINT_PAGE;
  // alaska_show_backtrace();
  alaska_barrier_poll();
  // if (unlikely(alaska_should_safepoint)) {
  //   alaska_barrier_poll();
  // }
  // // Simply load from the safepoint page. If we have a barrier pending, this will fault into the
  // segfault handler. uint64_t res = *(volatile uint64_t *)ALASKA_SAFEPOINT_PAGE; (void)res;
}



extern "C" void alaska_barrier_before_escape(void) {
  atomic_inc(alaska_thread_state.escaped, 1);
  // alaska_thread_state.escaped = 1;
  // printf("before!\n");
}

extern "C" void alaska_barrier_after_escape(void) {
  atomic_dec(alaska_thread_state.escaped, 1);
  // printf("after!\n");
}
