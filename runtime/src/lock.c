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
 * Note: This file is inlined by the compiler to make locks faster. Do not declare any global variables here, as they
 * may get overwritten or duplciated needlessly.
 */


extern uint64_t alaska_next_usage_timestamp;

#ifdef ALASKA_CLASS_TRACKING
void alaska_classify_track(uint8_t object_class);
#endif

// static ALASKA_INLINE unsigned long long extract(unsigned long long ptr) {
// 	unsigned long long ret = 0x7fffffff;
// 	asm ("pext %0, %1, %2\n\t"
//             : "=r"(ret)
//             : "r"(ptr), "r"(ret) : );
// 	return ret;
// }

alaska_mapping_t *alaska_get_mapping(void *restrict ptr) {
	// int64_t i = (int64_t)ptr;
	// if (i < 0) return (alaska_mapping_t *)(uint64_t)extract(i);
	// return NULL;
  handle_t h;
  h.ptr = ptr;
  if (likely(h.flag != 0)) {
    return (alaska_mapping_t *)(uint64_t)h.handle;
  }
  return NULL;
}



// Once you have a mapping entry for a handle, compute the pointer it should use.
ALASKA_INLINE void *alaska_translate(void *restrict ptr, alaska_mapping_t *m) {
  handle_t h;
  h.ptr = ptr;
  ALASKA_SANITY(h.offset <= m->size,
      "out of bounds access.\nAttempt to access offset %u in an "
      "object of size %u. Handle = %p",
      h.offset, m->size, ptr);
  return (void *)((uint64_t)m->ptr + h.offset);
}

ALASKA_INLINE void alaska_track_access(alaska_mapping_t *m) {
#ifdef ALASKA_CLASS_TRACKING
  alaska_classify_track(m->object_class);
#endif

  atomic_get_inc(alaska_next_usage_timestamp, m->usage_timestamp, 1);
  // m->usage_timestamp = alaska_next_usage_timestamp++;
}

ALASKA_INLINE void alaska_track_lock(alaska_mapping_t *m) {
  atomic_inc(m->locks, 1);
  // m->locks++;
}

ALASKA_INLINE void alaska_track_unlock(alaska_mapping_t *m) {
  atomic_dec(m->locks, 1);
  // m->locks--;
}

ALASKA_INLINE void *alaska_lock(void *restrict ptr) {
  alaska_mapping_t *m = alaska_get_mapping(ptr);
  if (m == NULL) return ptr;
  alaska_track_access(m);
  alaska_track_lock(m);
  return alaska_translate(ptr, m);
}

ALASKA_INLINE void alaska_unlock(void *restrict ptr) {
  alaska_mapping_t *m = alaska_get_mapping(ptr);
  if (m == NULL) return;
  alaska_track_unlock(m);
}

