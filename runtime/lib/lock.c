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
#define __alaska_get_INLINE

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
// helps implementation and performance (inline stuff in alaska_get in lock.c)
#ifndef ALASKA_SERVICE_ON_GET
#define ALASKA_SERVICE_ON_GET(mapping)  // ... nothing ...
#endif

#ifndef ALASKA_SERVICE_ON_PUT
#define ALASKA_SERVICE_ON_PUT(mapping)  // ... nothing ...
#endif

/**
 * Note: This file is inlined by the compiler to make locks faster.
 * Do not declare any global variables here, as they may get overwritten
 * or duplciated needlessly. (Which can lead to linker errors)
 */


// #define ROOT ((alaska_mapping_t *)(uint64_t)0)

// alaska_mapping_t *new_translate(void *restrict ptr) {
//   handle_t h;
//   h.ptr = ptr;
//   if (unlikely(h.flag == 0)) {
//     return NULL;
//   }

//   alaska_mapping_t *m = (alaska_mapping_t *)(h.handle * sizeof(alaska_mapping_t));
//   // #ifdef ALASKA_SWAP_SUPPORT
//   //   if (unlikely(m->ptr == NULL)) alaska_ensure_present(m);
//   // #endif
//   return (void *)((uint64_t)m->ptr + h.offset);
// }

// alaska_mapping_t *old_translate(void *restrict ptr) {
//   handle_t h;
//   h.ptr = ptr;
//   if (unlikely(h.flag == 0)) {
//     return NULL;
//   }
//   alaska_mapping_t *m = (alaska_mapping_t *)h.handle;
//   // #ifdef ALASKA_SWAP_SUPPORT
//   //   if (unlikely(m->ptr == NULL)) alaska_ensure_present(m);
//   // #endif
//   return (void *)((uint64_t)m->ptr + h.offset);
// }

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


#ifdef ALASKA_SWAP_SUPPORT
  if (unlikely(m->ptr == NULL)) alaska_ensure_present(m);
#endif

	ALASKA_SANITY(m->ptr != NULL, "Handle has no pointer!");



  handle_t h;
  h.ptr = ptr;
  return (void *)((uint64_t)m->ptr + h.offset);
#endif
}



ALASKA_INLINE void *alaska_get(void *restrict ptr) {
  alaska_mapping_t *m = alaska_lookup(ptr);
  if (unlikely(m == NULL)) return ptr;


  // finally, translate
  void *out = alaska_translate(ptr, m);
  ALASKA_SERVICE_ON_GET(m);
  return out;
}

ALASKA_INLINE void alaska_put(void *restrict ptr) {
  alaska_mapping_t *m = alaska_lookup(ptr);
  if (unlikely(m == NULL)) return;

  ALASKA_SERVICE_ON_PUT(m);
}

static int needs_barrier = 0;
void alaska_time_hook_fire(void) {
  // printf("checking for barrier!\n");
  if (needs_barrier) {
    printf("needs barrier!\n");
  }
}