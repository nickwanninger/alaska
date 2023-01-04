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

static uint64_t next_usage_timestamp = 0;
#ifdef ALASKA_CLASS_TRACKING
static uint64_t alaska_class_access_counts[256];
#endif

static uint64_t foo;

void alaska_classify(void* ptr, uint8_t c) {
	foo += 1;
#ifdef ALASKA_CLASS_TRACKING
  handle_t h;
  h.ptr = ptr;
  if (likely(h.flag != 0)) {
    alaska_mapping_t* ent = GET_ENTRY(ptr);
    ent->object_class = c;
  }
#endif
}

#ifdef ALASKA_CLASS_TRACKING
void alaska_classify_init(void) {}

void alaska_classify_deinit(void) {
  if (getenv("ALASKA_DUMP_OBJECT_CLASSES") != NULL) {
    printf("class,accesses\n");
    for (int i = 0; i < 256; i++) {
      if (alaska_class_access_counts[i] != 0) {
        printf("%d,%zu\n", i, alaska_class_access_counts[i]);
      }
    }
  }
}
#endif


enum alaska_fault_reason {
  NOT_PRESENT,
  OUT_OF_BOUNDS,
};

// Alaska's version of a soft page fault. Note the "noinline". This is because
// this is a slow path, and we don't really care about performance, as this
// function may go out to disk, decompress, decrypt, or something else more
// interesting :)
//
// This, and alaska_barrier are the main locations where the runtime can be
// customized
__declspec(noinline) void alaska_fault(alaska_mapping_t* ent, enum alaska_fault_reason reason, off_t offset) {
  if (reason == OUT_OF_BOUNDS) {
    fprintf(stderr,
        "[FATAL] alaska: out of bound access of handle %ld. Attempt to "
        "access byte %zu in a %d byte handle!\n",
        alaska_table_get_canonical(ent), offset, ent->size);
    exit(-1);
  }

  // if (reason == NOT_PRESENT) {
  //   // lazy allocate for now.
  //   return;
  // }

  fprintf(
      stderr, "[FATAL] alaska: unknown fault reason, %d, on handle %ld.\n", reason, alaska_table_get_canonical(ent));
  exit(-1);
}


alaska_mapping_t *alaska_get_mapping(void *restrict ptr) {
  handle_t h;
  h.ptr = ptr;
  if (likely(h.flag != 0)) {
 		return (alaska_mapping_t*)(uint64_t)h.handle;
	}
	return NULL;
}

void *alaska_translate(void *restrict ptr, alaska_mapping_t *m) {
  handle_t h;
  h.ptr = ptr;
  ALASKA_SANITY(h.offset <= m->size,
      "out of bounds access.\nAttempt to access offset %u in an "
      "object of size %u. Handle = %p",
      h.offset, m->size, ptr);
  return (void*)((uint64_t)m->ptr + h.offset);
}

void alaska_track_access(alaska_mapping_t *m) {
#ifdef ALASKA_CLASS_TRACKING
  alaska_class_access_counts[ent->object_class]++;
#endif
  m->usage_timestamp = next_usage_timestamp++;
}
void alaska_track_lock(alaska_mapping_t *m) {
  m->locks++;
}
void alaska_track_unlock(alaska_mapping_t *m) {
  m->locks--;
}

// These functions are simple wrappers around the guarded version of the
// same name. These versions just check if `ptr` is a handle before locking.
__declspec(alwaysinline) void* alaska_lock(void* restrict ptr) {
	alaska_mapping_t *m = alaska_get_mapping(ptr);
	if (m == NULL) return ptr;

	alaska_track_access(m);
	alaska_track_lock(m);
	return alaska_translate(ptr, m);
}

void alaska_unlock(void* restrict ptr) {
	alaska_mapping_t *m = alaska_get_mapping(ptr);
	if (m == NULL) return;
	alaska_track_unlock(m);
}

// This function exists to lock on extern escapes.
// TODO: determine if we should lock it forever or not...
static uint64_t escape_locks = 0;
void* alaska_lock_for_escape(void* ptr) {
  handle_t h;
  h.ptr = ptr;
  if (likely(h.flag != 0)) {
    atomic_inc(escape_locks, 1);
    // escape_locks++;
    // its easier to do this than to duplicate efforts and inline.
    void* t = alaska_lock(ptr);
    alaska_unlock(ptr);
    return t;
  }
  return ptr;
}
