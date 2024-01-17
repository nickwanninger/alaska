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
#include <execinfo.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/cdefs.h>
#include <sys/mman.h>
#include <unistd.h>

#include <alaska.h>
#include <alaska/alaska.hpp>
#include <alaska/config.h>
#include <alaska/utils.h>
#include <dlfcn.h>

/**
 * Note: This file is inlined by the compiler to make locks faster.
 * Do not declare any global variables here, as they may get overwritten
 * or duplciated needlessly. (Which can lead to linker errors)
 */

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


// TODO: we don't use this anymore. Do we need it?
void *alaska_translate_escape(void *ptr) {
  if (ptr == (void *)-1UL) {
    return ptr;
  }
  return alaska_translate(ptr);
}


static __attribute_noinline__ void track(uintptr_t handle) {
  fprintf(stderr, "tr %p\n", handle);
}

void *alaska_translate(void *ptr) {
  int64_t bits = (int64_t)ptr;
  if (unlikely(bits >= 0 || bits == -1)) {
    return ptr;
  }
  // if (unlikely(bits >= 0)) {
  //   return ptr;
  // }

  // Grab the mapping from the runtime
  auto m = alaska::Mapping::from_handle(ptr);

  // Grab the pointer
  void *mapped = m->get_pointer();
  // Apply the offset from the pointer
  ptr = (void *)((uint64_t)mapped + (uint32_t)bits);
  return ptr;
}

extern bool alaska_should_safepoint;
extern "C" uint64_t alaska_barrier_poll();

extern "C" void alaska_safepoint(void) {
  alaska_barrier_poll();
}
