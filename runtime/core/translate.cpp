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



extern "C" {
extern int __LLVM_StackMaps __attribute__((weak));
}

#define APPLY_OFFSET(mapped, bits) \
  (void *)((uint64_t)mapped + ((uint64_t)bits & ((1LU << ALASKA_SIZE_BITS) - 1)))

extern "C" void *alaska_translate_uncond(void *ptr) {
  int64_t bits = (int64_t)ptr;

  auto m = alaska::Mapping::from_handle(ptr);
  // Pull the address from the mapping
  void *mapped = m->get_pointer();
  ptr = APPLY_OFFSET(mapped, bits);
  return ptr;
}

// TODO: we don't use this anymore. Do we need it?
void *alaska_translate_escape(void *ptr) {
  if (ptr == (void *)-1UL) {
    return ptr;
  }
  return alaska_translate(ptr);
}



#ifdef ALASKA_HTLB_SIM
extern void alaska_htlb_sim_track(uintptr_t handle);
#endif




void *alaska_translate(void *ptr) {
#ifdef ALASKA_HTLB_SIM
  alaska_htlb_sim_track((uintptr_t)ptr);
#endif

  int64_t bits = (int64_t)ptr;
  if (unlikely(bits >= 0 || bits == -1)) {
    return ptr;
  }


  // Grab the mapping from the runtime
  auto m = alaska::Mapping::from_handle(ptr);


  // Grab the pointer
  void *mapped = m->get_pointer();
  // Apply the offset from the pointer
  ptr = APPLY_OFFSET(mapped, bits);
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
  start = end = 0;
  start = (uint64_t)rbp;
  while ((uint64_t)rbp > 0x1000) {
    end = (uint64_t)rbp;
    if (end > base_start) base_start = end;
    rbp = *(void **)rbp;  // Follow the chain of rbp values
  }
  printf(" (%zd)\n", end - start);
}

extern bool alaska_should_safepoint;
extern "C" uint64_t alaska_barrier_poll();

extern "C" void alaska_safepoint(void) { alaska_barrier_poll(); }

// TODO:
extern "C" void *__alaska_leak(void *ptr) { return alaska_translate(ptr); }
