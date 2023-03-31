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
#define __ALASKA_LOCK_INLINE

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
// helps implementation and performance (inline stuff in alaska_lock in lock.c)
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

ALASKA_INLINE alaska_mapping_t *alaska_lookup(void *restrict ptr) {
#ifdef ALASKA_SIM_MODE
  extern alaska_mapping_t *sim_lookup(void *restrict ptr);
  return sim_lookup(ptr);
#else
  handle_t h;
  h.ptr = ptr;
  if (unlikely(h.flag == 0)) {
    return NULL;
  }
  return (alaska_mapping_t *)(uint64_t)h.handle;
#endif
}

extern void alaska_swap_in(alaska_mapping_t *m);

ALASKA_INLINE void *alaska_translate(void *restrict ptr, alaska_mapping_t *m) {
#ifdef ALASKA_SIM_MODE
  extern void *sim_translate(void *restrict ptr);
  return sim_translate(ptr);
#else


#ifdef ALASKA_SWAP_SUPPORT
  if (unlikely(m->ptr == NULL)) alaska_swap_in(m);
#endif

	ALASKA_SANITY(m->ptr != NULL, "Handle has no pointer!");


  handle_t h;
  h.ptr = ptr;
  return (void *)((uint64_t)m->ptr + h.offset);
#endif
}

ALASKA_INLINE void *alaska_lock(void *restrict ptr) {
  alaska_mapping_t *m = alaska_lookup(ptr);
  if (unlikely(m == NULL)) return ptr;

#ifdef ALASKA_CLASS_TRACKING
  alaska_classify_track(m->object_class);
#endif


  // Do some service stuff *before* translating.
  ALASKA_SERVICE_ON_LOCK(m);
  // finally, translate
  return alaska_translate(ptr, m);
}

ALASKA_INLINE void alaska_unlock(void *restrict ptr) {
  //
  alaska_mapping_t *m = alaska_lookup(ptr);
  if (unlikely(m == NULL)) return;

  ALASKA_SERVICE_ON_UNLOCK(m);
}
