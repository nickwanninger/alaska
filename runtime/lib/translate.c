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
#include <alaska/internal.h>
#include <alaska/config.h>

#ifdef ALASKA_SERVICE_ANCHORAGE
// include the inline implementation offered by the services
#include <alaska/service/anchorage.inline.h>
#endif


#ifdef ALASKA_SERVICE_NONE
// include the inline implementation offered by the services
#include <alaska/service/none.inline.h>
#endif


// This is the interface for services. It may seem like a hack, but it
// helps implementation and performance (inline stuff in alaska_translate in lock.c)
#ifndef ALASKA_SERVICE_ON_LOCK
#define ALASKA_SERVICE_ON_LOCK(mapping)  // ... nothing ...
#endif

#ifndef ALASKA_SERVICE_ON_UNLOCK
#define ALASKA_SERVICE_ON_UNLOCK(mapping)  // ... nothing ...
#endif

/**
 * Note: This file is inlined by the compiler to make locks faster.
 * Do not declare any global variables here, as they may get overwritten
 * or duplciated needlessly. (Which can lead to linker errors)
 */

extern int alaska_verify_is_locally_locked(void *ptr);


ALASKA_INLINE alaska_mapping_t *alaska_lookup(void *restrict ptr) {
  handle_t h;
  h.ptr = ptr;
  if (unlikely(h.flag == 0)) {
    return NULL;
  }
  return (alaska_mapping_t *)(uint64_t)h.handle;
}



ALASKA_INLINE void *alaska_translate(void *restrict ptr) {
  /**
   * This function is written in a strange way on purpose. It's written
   * with a close understanding of how it will be lowered into LLVM IR
   * (mainly how it will be lowered into PHI instructions).
   */


  // Sanity check that a handle has been locked on this thread
	// *before* translating. This ensures it won't be moved during
	// the invocation of this function
  ALASKA_SANITY(alaska_verify_is_locally_locked(ptr),
      "Pointer '%p' is not locked on the shadow stack before calling translate\n", ptr);

  handle_t h;
  h.ptr = ptr;
  if (likely(h.flag == 1)) {
    alaska_mapping_t *m = (alaska_mapping_t *)(h.handle);
    void *mapped = m->ptr;
#ifdef ALASKA_SWAP_SUPPORT
    if (unlikely(mapped == 0)) {  // is the top bit set?
      mapped = alaska_ensure_present(m);
    }
#endif
    ALASKA_SANITY(mapped != NULL, "Mapped pointer is null for handle %p\n", ptr);
    ptr = (void *)((uint64_t)mapped + h.offset);
  }
  return ptr;
}

ALASKA_INLINE void alaska_release(void *restrict ptr) {
  // This function is just a marker that `ptr` is now dead (no longer used)
  // and should not have any real meaning in the runtime
}

static int needs_barrier = 0;
void alaska_time_hook_fire(void) {
  // printf("checking for barrier!\n");
  if (needs_barrier) {
    printf("needs barrier!\n");
  }
}
