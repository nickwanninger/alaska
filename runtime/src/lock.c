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
  alaska_mapping_t *m = (alaska_mapping_t *)(uint64_t)h.handle;
  return m;
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

ALASKA_INLINE void *alaska_lock(void *restrict ptr) {
  alaska_mapping_t *m = alaska_lookup(ptr);
  if (m == NULL) return ptr;

  // Perform the lock
  // atomic_inc(m->locks, 1);
  m->locks++;

  // call personality *after* we lock
  // ALASKA_PERSONALITY_ON_LOCK(m);

  return alaska_translate(ptr, m);
}

ALASKA_INLINE void alaska_unlock(void *restrict ptr) {
  alaska_mapping_t *m = alaska_lookup(ptr);
  if (m == NULL) return;

  // Perform the unlock
  ALASKA_SANITY(m->locks > 0, "lock value is too low!");
  // atomic_dec(m->locks, 1);
  m->locks--;

  // call personality *after* we unlock
  // ALASKA_PERSONALITY_ON_UNLOCK(m);
}
