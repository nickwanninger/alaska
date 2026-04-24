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
#include <sys/cdefs.h>
#include <sys/mman.h>
#include <unistd.h>

#include <alaska.h>
#include <alaska/alaska.hpp>
#include <alaska/config.h>
#include <alaska/util/utils.h>
#include <alaska/util/Logger.hpp>
#include <alaska/core/Runtime.hpp>
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



#define ENABLE_HANDLE_FAULTS


extern "C" __attribute__((always_inline)) void *alaska_translate_uncond(void *ptr) {
  auto m = alaska::Mapping::from_handle(ptr);


  // #ifdef ENABLE_HANDLE_FAULTS
  // Check if either of the two top bits are set (pending_fault or access_trace)
  bool shouldFault = m->should_software_fault();
  if (unlikely(shouldFault)) {
    return alaska::do_handle_fault_and_translate((int64_t)ptr);
  }
  // #endif

  // Read the HTE
  void *mapped = m->get_pointer();
  // Apply the offset from the pointer
  void *result = APPLY_OFFSET(mapped, (int64_t)ptr);

  return result;
}

extern "C" void *alaska_translate(void *ptr) {
#ifdef ALASKA_HTLB_SIM
  alaska_htlb_sim_track((uintptr_t)ptr);
#endif

  int64_t bits = (int64_t)ptr;
  if (unlikely(bits >= 0 || bits == -1)) {
    // translate_miss++;
    return ptr;
  }

  // translate_hit++;


  return alaska_translate_uncond(ptr);
}




extern "C" void *alaska_translate_nop(void *p) { return p; }

void alaska_release(void *ptr) {
  // This function is just a marker that `ptr` is now dead (no longer used)
  // and should not have any real meaning in the runtime
}

extern "C" uint64_t alaska_barrier_poll();
extern "C" void alaska_safepoint(void) { alaska_barrier_poll(); }

// TODO:
extern "C" void *__alaska_leak(void *ptr) { return alaska_translate(ptr); }
