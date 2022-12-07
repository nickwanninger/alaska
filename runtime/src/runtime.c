#include <alaska.h>
#include <alaska/internal.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <jemalloc/jemalloc.h>

#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)




// convert a canonical handle id to a pointer.
#define FROM_CANONICAL(chid) (&alaska_map[chid])
// How many map cells are there in the map table?
size_t alaska_map_size = 0;
// The memory for the alaska translation map. This lives at 0x200000 and is grown in 2mb chunks
alaska_mapping_t *alaska_map = NULL;

// ...
static uint64_t next_usage_timestamp = 0;



// The alaska allocator is a *very* simple free list allocator. We can do
// this because all handle map entries  are the same size. It is the
// allocations they point to that are variable sized. This means we can
// simply store a linked list of free nodes within the alaska map entrys
// themselves. This turns `halloc` calls into a nice O(1) common case,
// and a `mremap()` call in the worst case (if we need more handles)
static alaska_mapping_t *next_handle = NULL;

__declspec(noinline) void *halloc(size_t sz) {
  assert(sz < (1LLU << 32));
  uint64_t handle = HANDLE_MARKER | (((uint64_t)next_handle) << 32);

  alaska_mapping_t *ent = next_handle;
  next_handle = (alaska_mapping_t *)ent->ptr;
  if (next_handle == NULL) {
    fprintf(stderr, "alaska: out of space!\n");
    exit(-1);
  }

  memset(ent, 0, MAP_ENTRY_SIZE);
  ent->size = sz;
  ent->ptr = je_calloc(1, sz);  // just use malloc
  return (void *)handle;
}

__declspec(noinline) void *hcalloc(size_t nmemb, size_t size) { return halloc(nmemb * size); }

// Reallocate a handle
__declspec(noinline) void *hrealloc(void *handle, size_t sz) {
  alaska_mapping_t *ent = GET_ENTRY(handle);
  ent->ptr = je_realloc(ent->ptr, sz);
  ent->size = sz;
  // realloc in alaska will always return the same pointer...
  // Ahh the benefits of using handles!

  // hopefully, nobody is relying on the output of realloc changing :I
  return handle;
}


__declspec(noinline) void hfree(void *ptr) {
  alaska_mapping_t *ent = GET_ENTRY(ptr);
  // assert(ent->locks == 0);
  je_free(ent->ptr);
  ent->size = 0;
  ent->ptr = (void *)next_handle;

  next_handle = (alaska_mapping_t *)ent;
}


/* Use 'MADV_DONTNEED' to release memory to operating system quickly.
 * We do that in a fork child process to avoid CoW when the parent modifies
 * these shared pages. */
void alaska_madvise_dontneed(alaska_mapping_t *ent) {
#if defined(__linux__)
  const size_t page_size = 4096;
  size_t page_size_mask = page_size - 1;
  void *ptr = ent->ptr;
  size_t real_size = ent->size;
  if (real_size < page_size) return;

  /* we need to align the pointer upwards according to page size, because
   * the memory address is increased upwards and we only can free memory
   * based on page. */
  char *aligned_ptr = (char *)(((size_t)ptr + page_size_mask) & ~page_size_mask);
  real_size -= (aligned_ptr - (char *)ptr);
  if (real_size >= page_size) {
    madvise((void *)aligned_ptr, real_size & ~page_size_mask, MADV_DONTNEED);
  }
#endif
}


void *alaska_defrag_alloc(alaska_mapping_t *ent) {
  void *ptr = ent->ptr;
  size_t size = ent->size;
  void *newptr;
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


// Alaska's version of a soft page fault. Note the "noinline". This is because this
// is a slow path, and we don't really care about performance, as this function
// may go out to disk, decompress, decrypt, or something else more interesting :)
//
// This, and alaska_barrier are the main locations where the runtime can be customized
__declspec(noinline) void alaska_fault(alaska_mapping_t *ent, enum alaska_fault_reason reason, off_t offset) {
  if (reason == OUT_OF_BOUNDS) {
    fprintf(stderr, "[FATAL] alaska: out of bound access of handle %ld. Attempt to access byte %zu in a %d byte handle!\n",
        ENT_GET_CANONICAL(ent), offset, ent->size);
    exit(-1);
  }


  if (reason == NOT_PRESENT) {
    // lazy allocate for now.
    ent->ptr = malloc(ent->size);
    return;
  }

  fprintf(stderr, "[FATAL] alaska: unknown fault reason, %d, on handle %ld.\n", reason, ENT_GET_CANONICAL(ent));
  exit(-1);
}

#define MAX_MOVE 32

int alaska_ent_usage_compare(const void *_a, const void *_b) {
  alaska_mapping_t *a = *(alaska_mapping_t **)_a;
  alaska_mapping_t *b = *(alaska_mapping_t **)_b;

  return a->usage_timestamp - b->usage_timestamp;
}


int alaska_ent_ptr_compare(const void *_a, const void *_b) {
  void *a = *(void **)_a;
  void *b = *(void **)_b;
  return (off_t)a - (off_t)b;
}

// This just means that the application has decided it is willing to let alaska
// potentially stop the world and do what it needs to do. This function doesn't
// do anything yet, but it could :)
__declspec(noinline) void alaska_barrier(void) {}


// The core function to lock a handle. This is called only once we know
// that @ptr is a handle (indicated by the top bit being set to 1).
void *alaska_guarded_lock(void *ptr) {
  // A handle is encoded as two 32 bit components. The first (top half)
  // component is simply a pointer to the mapping directly. The second
  // (bottom 32 bits) are an offset into the mapping. This simplifies
  // translation quite a bit, as all handles share the same bit pattern
  // and offset computation is very easy.
  //
  // On AAarch64, this operation can be performed with 3 instructions:
  //   ubfx; ldr; add;
  // (Extract bits (32..63), load `ent->ptr`, and add the offset)
  alaska_mapping_t *ent = GET_ENTRY(ptr);
  ALASKA_SANITY(ent >= alaska_map, "Entry is before the map");
  ALASKA_SANITY(ent < alaska_map + alaska_map_size, "Entry is after the map");

  // Extract the lower 32 bits of the pointer for the offset.
  off_t off = GET_OFFSET(ptr);

  // if the pointer is NULL, we need to perform a "handle fault" This
  // is to allow the runtime to fully deallocate unused handles, but it
  // is a relatively expensive check on some architectures...
  // if (unlikely(ent->ptr == NULL)) {
  //   alaska_fault(ent, NOT_PRESENT, off);
  // }

  // Proxy for temporal locality.
  // The compiler could decide to do this or not based on if it
  // if wants to track this in some scope
  ent->usage_timestamp = next_usage_timestamp++;

  // printf("lock %p, %ld\n", ptr, GET_CANONICAL(ptr));

  // Record the lock occuring so the runtime knows not to relocate the memory.
  // TODO: hoist this into the compiler and avoid doing it if it's not needed.
  // ex: if there are no calls to alaska_barrier between lock/unlocks,
  //     there is no need to lock (we only care about one thread for now)
  ent->locks++;


  // Return the address of the pointer plus the offset we are locking at.
  return (void *)((uint64_t)ent->ptr + off);
}


void alaska_guarded_unlock(void *ptr) {
  alaska_mapping_t *ent = GET_ENTRY(ptr);
  ent->locks--;
}


// These functions are simple wrappers around the guarded version of the
// same name. These versions just check if `ptr` is a handle before locking.
void *alaska_lock(void *ptr) {
  uint64_t h = (uint64_t)ptr;
  if (likely((h & HANDLE_MARKER) != 0)) {
    return alaska_guarded_lock(ptr);
  }
  return ptr;
}

void alaska_unlock(void *ptr) {
  uint64_t h = (uint64_t)ptr;
  if (likely((h & HANDLE_MARKER) != 0)) {
    alaska_guarded_unlock(ptr);
  }
}

// This function exists to lock on extern escapes.
// TODO: determine if we should lock it forever or not...
static uint64_t escape_locks = 0;
void *alaska_lock_for_escape(void *ptr) {
  uint64_t h = (uint64_t)ptr;
  if (unlikely((h & HANDLE_MARKER) != 0)) {
    atomic_inc(escape_locks, 1);
    // escape_locks++;
    // its easier to do this than to duplicate efforts and inline.
    void *t = alaska_lock(ptr);
    alaska_unlock(ptr);
    return t;
  }
  return ptr;
}


#define MAP_HUGE_2MB (21 << MAP_HUGE_SHIFT)

static void __attribute__((constructor)) alaska_init(void) {
  int fd = -1;
  int flags = MAP_PRIVATE | MAP_ANONYMOUS;
  size_t sz = MAP_GRANULARITY * 8;

  // TODO: do this using hugetlbfs :)
  alaska_map = (alaska_mapping_t *)mmap((void *)MAP_GRANULARITY, sz, PROT_READ | PROT_WRITE, flags | MAP_FIXED, fd, 0);
  alaska_map_size = sz / MAP_ENTRY_SIZE;

  for (size_t i = 0; i < alaska_map_size - 1; i++) {
    // when unmapped, the pointer field points to the next free handle
    alaska_map[i].ptr = (void *)&alaska_map[i + 1];
    alaska_map[i].size = 0;  // a size of zero indicates that this field is unmapped
  }
  alaska_map[alaska_map_size - 1].ptr = NULL;
  // start allocating at the first handle
  next_handle = &alaska_map[0];
}

static void __attribute__((destructor)) alaska_deinit(void) {
  // Simply unmap the map region
  munmap(alaska_map, alaska_map_size * MAP_ENTRY_SIZE);
}
