#include <alaska.h>
#include <alaska/internal.h>
#include <assert.h>
#include <errno.h>
#include <execinfo.h>
#include <fcntl.h>
#include <jemalloc/jemalloc.h>
#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)
#define round_up(x, y) (((x) + (y)-1) & ~((y)-1))

typedef union {
  struct {
    unsigned offset : 32;  // the offset into the handle
    unsigned handle : 31;  // the translation in the translation table
    unsigned flag : 1;     // the high bit indicates the `ptr` is a handle
  };
  void* ptr;
} handle_t;

#ifdef ALASKA_CLASS_TRACKING
static uint64_t alaska_class_access_counts[256];
#endif

// ...
static uint64_t next_usage_timestamp = 0;

__declspec(noinline) void* halloc(size_t sz) {
  assert(sz < (1LLU << 32));
  alaska_mapping_t* ent = alaska_table_get();
  uint64_t handle = HANDLE_MARKER | (((uint64_t)ent) << 32);

  if (ent == NULL) {
    fprintf(stderr, "alaska: out of space!\n");
    exit(-1);
  }

  ent->size = sz;
  ent->ptr = je_calloc(1, sz);  // just use malloc
                                //
#ifdef ALASKA_CLASS_TRACKING
  ent->object_class = 0;
#endif
  return (void*)handle;
}

size_t alaska_usable_size(void* ptr) {
  uint64_t h = (uint64_t)ptr;
  if (unlikely((h & HANDLE_MARKER) != 0)) {
    return GET_ENTRY(h)->size;
  }

  return malloc_usable_size(ptr);
}

__declspec(noinline) void* hcalloc(size_t nmemb, size_t size) { return halloc(nmemb * size); }

// Reallocate a handle
__declspec(noinline) void* hrealloc(void* handle, size_t sz) {
  if (handle == NULL) return halloc(sz);
  uint64_t h = (uint64_t)handle;

  if (unlikely((h & HANDLE_MARKER) == 0)) {
    return realloc(handle, sz);
  }
  alaska_mapping_t* ent = GET_ENTRY(handle);
  ent->ptr = je_realloc(ent->ptr, sz);
  ent->size = sz;
  // realloc in alaska will always return the same pointer...
  // Ahh the benefits of using handles!

  // hopefully, nobody is relying on the output of realloc changing :^)
  return handle;
}

__declspec(noinline) void hfree(void* ptr) {
  if (ptr == NULL) return;

  uint64_t h = (uint64_t)ptr;
  if (unlikely((h & HANDLE_MARKER) == 0)) {
    return free(ptr);
  }

  alaska_mapping_t* ent = GET_ENTRY(ptr);
  // assert(ent->locks == 0);
  je_free(ent->ptr);
  // return the mapping to the table
  alaska_table_put(ent);
}

/* Use 'MADV_DONTNEED' to release memory to operating system quickly.
 * We do that in a fork child process to avoid CoW when the parent modifies
 * these shared pages. */
void alaska_madvise_dontneed(alaska_mapping_t* ent) {
#if defined(__linux__)
  const size_t page_size = 4096;
  size_t page_size_mask = page_size - 1;
  void* ptr = ent->ptr;
  size_t real_size = ent->size;
  if (real_size < page_size) return;

  /* we need to align the pointer upwards according to page size, because
   * the memory address is increased upwards and we only can free memory
   * based on page. */
  char* aligned_ptr = (char*)(((size_t)ptr + page_size_mask) & ~page_size_mask);
  real_size -= (aligned_ptr - (char*)ptr);
  if (real_size >= page_size) {
    madvise((void*)aligned_ptr, real_size & ~page_size_mask, MADV_DONTNEED);
  }
#endif
}

void* alaska_defrag_alloc(alaska_mapping_t* ent) {
  void* ptr = ent->ptr;
  size_t size = ent->size;
  void* newptr;
  // if(!je_get_defrag_hint(ptr)) {
  //     server.stat_active_defrag_misses++;
  //     return NULL;
  // }

  /* move this allocation to a new allocation.
   * make sure not to use the thread cache. so that we don't get back the same
   * pointers we try to free */
  // size = je_malloc_size(ptr);
  newptr = je_mallocx(size, MALLOCX_TCACHE_NONE);
  memcpy(newptr, ptr, size);
  je_free(ptr);
  ent->ptr = newptr;
  return newptr;
}

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

#define MAX_MOVE 32

int alaska_ent_usage_compare(const void* _a, const void* _b) {
  alaska_mapping_t* a = *(alaska_mapping_t**)_a;
  alaska_mapping_t* b = *(alaska_mapping_t**)_b;

  return a->usage_timestamp - b->usage_timestamp;
}

int alaska_ent_ptr_compare(const void* _a, const void* _b) {
  void* a = *(void**)_a;
  void* b = *(void**)_b;
  return (off_t)a - (off_t)b;
}

// This just means that the application has decided it is willing to let alaska
// potentially stop the world and do what it needs to do. This function doesn't
// do anything yet, but it could :)
__declspec(noinline) void alaska_barrier(void) {}

// The core function to lock a handle. This is called only once we know
// that @ptr is a handle (indicated by the top bit being set to 1).
void* alaska_guarded_lock(void* restrict ptr) {
  handle_t h;
  h.ptr = ptr;
  alaska_mapping_t* ent = (alaska_mapping_t*)(uint64_t)h.handle;

#ifdef ALASKA_CLASS_TRACKING
  alaska_class_access_counts[ent->object_class]++;
#endif

  // if the pointer is NULL, we need to perform a "handle fault" This
  // is to allow the runtime to fully deallocate unused handles, but it
  // is a relatively expensive check on some architectures...
  if (unlikely(ent->ptr == NULL)) {
    alaska_fault(ent, NOT_PRESENT, h.offset);
  }

  ALASKA_SANITY(h.offset <= ent->size,
      "out of bounds access.\nAttempt to access offset %u in an "
      "object of size %u. Handle = %p",
      h.offset, ent->size, ptr);

  ent->usage_timestamp = next_usage_timestamp++;
  ent->locks++;

  // Return the address of the pointer plus the offset we are locking at.
  return (void*)((uint64_t)ent->ptr + h.offset);
}

void alaska_guarded_unlock(void* restrict ptr) {
  handle_t h;
  h.ptr = ptr;
  alaska_mapping_t* ent = (alaska_mapping_t*)(uint64_t)h.handle;
  ent->locks--;
}

// These functions are simple wrappers around the guarded version of the
// same name. These versions just check if `ptr` is a handle before locking.
__declspec(alwaysinline) void* alaska_lock(void* restrict ptr) {
  handle_t h;
  h.ptr = ptr;
  if (likely(h.flag != 0)) {
    return alaska_guarded_lock(ptr);
  }
  return ptr;
}

void alaska_unlock(void* restrict ptr) {
  handle_t h;
  h.ptr = ptr;
  if (likely(h.flag != 0)) {
    alaska_guarded_unlock(ptr);
  }
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

void alaska_classify(void* ptr, uint8_t c) {
#ifdef ALASKA_CLASS_TRACKING
  handle_t h;
  h.ptr = ptr;
  if (likely(h.flag != 0)) {
    alaska_mapping_t* ent = GET_ENTRY(ptr);
    ent->object_class = c;
  }
#endif
}

static void __attribute__((constructor)) alaska_init(void) {
  // initialize the translation table first
  alaska_table_init();
}

static void __attribute__((destructor)) alaska_deinit(void) {
  // Simply unmap the map region

#ifdef ALASKA_CLASS_TRACKING
  if (getenv("ALASKA_DUMP_OBJECT_CLASSES") != NULL) {
    printf("class,accesses\n");
    for (int i = 0; i < 256; i++) {
      if (alaska_class_access_counts[i] != 0) {
        printf("%d,%zu\n", i, alaska_class_access_counts[i]);
      }
    }
  }
#endif

  // delete the table at the end
  alaska_table_deinit();
}

#define BT_BUF_SIZE 100
void alaska_dump_backtrace(void) {
  return;
  int nptrs;

  void* buffer[BT_BUF_SIZE];
  char** strings;

  nptrs = backtrace(buffer, BT_BUF_SIZE);
  printf("Backtrace:\n", nptrs);

  /* The call backtrace_symbols_fd(buffer, nptrs, STDOUT_FILENO)
     would produce similar output to the following: */

  strings = backtrace_symbols(buffer, nptrs);
  if (strings == NULL) {
    perror("backtrace_symbols");
    exit(EXIT_FAILURE);
  }

  for (int j = 0; j < nptrs; j++)
    printf("\x1b[92m%d\x1b[0m: %s\n", j, strings[j]);

  free(strings);
}
