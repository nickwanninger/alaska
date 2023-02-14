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
// include the inline implementation offered by the personalities
#include <alaska/service/anchorage.inline.h>
#endif


#ifdef ALASKA_SERVICE_NONE
// include the inline implementation offered by the personalities
#include <alaska/service/none.inline.h>
#endif


// This is the interface for personalities. It may seem like a hack, but it
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
 * or duplciated needlessly.
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

ALASKA_INLINE void *alaska_translate(void *restrict ptr, alaska_mapping_t *m) {
#ifdef ALASKA_SIM_MODE
  extern void *sim_translate(void *restrict ptr);
  return sim_translate(ptr);
#else
  handle_t h;
  h.ptr = ptr;
  return (void *)((uint64_t)m->ptr + h.offset);
#endif
}


ALASKA_INLINE void *alaska_do_lock(alaska_mapping_t *m, void *restrict ptr) {
  ALASKA_SERVICE_ON_LOCK(m);
  // finally, translate
  return alaska_translate(ptr, m);
}

ALASKA_INLINE void alaska_do_unlock(alaska_mapping_t *m, void *restrict ptr) {
  // call personality *after* we unlock
  ALASKA_SERVICE_ON_UNLOCK(m);
}


ALASKA_INLINE void *alaska_lock(void *restrict ptr) {
  alaska_mapping_t *m = alaska_lookup(ptr);
  if (unlikely(m == NULL)) return ptr;

#ifdef ALASKA_CLASS_TRACKING
  alaska_classify_track(m->object_class);
#endif

  return alaska_do_lock(m, ptr);
}

ALASKA_INLINE void alaska_unlock(void *restrict ptr) {
  alaska_mapping_t *m = alaska_lookup(ptr);
  if (unlikely(m == NULL)) return;

  // __builtin_prefetch(m->ptr);

  alaska_do_unlock(m, ptr);
}
