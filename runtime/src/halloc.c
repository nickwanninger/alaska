#include <alaska.h>
#include <alaska/internal.h>
#include <assert.h>
#include <jemalloc/jemalloc.h>
#include <malloc.h>
#include <string.h>
#include <sys/mman.h>


static long high_watermark = 0;
static long total_size = 0;
static long halloc_calls = 0;
static long hrealloc_calls = 0;
static long hfree_calls = 0;

static void update_memory_used(long delta) {
  total_size += delta;
  if (total_size > high_watermark) {
    high_watermark = total_size;
  }
}


void alaska_halloc_init(void) {
  //
}
void alaska_halloc_deinit(void) {
  if (getenv("HALLOCSTATS") != NULL) {
    fprintf(stderr, "halloc calls        : %zu\n", halloc_calls);
    fprintf(stderr, "hralloc calls       : %zu\n", hrealloc_calls);
    fprintf(stderr, "hfree calls         : %zu\n", hfree_calls);
    fprintf(stderr, "High Water Mark     : %zu\n", high_watermark);
  }
}


size_t alaska_usable_size(void *ptr) {
  uint64_t h = (uint64_t)ptr;
  if (unlikely((h & HANDLE_MARKER) != 0)) {
    return GET_ENTRY(h)->size;
  }
  return malloc_usable_size(ptr);
}



void *alaska_defrag(alaska_mapping_t *ent) {
  void *ptr = ent->ptr;
  size_t size = ent->size;
  void *newptr;
  /* move this allocation to a new allocation.
   * make sure not to use the thread cache. so that we don't get back the same
   * pointers we try to free */
  newptr = je_mallocx(size, MALLOCX_TCACHE_NONE);
  // printf("%zd\n", (char *)newptr - (char *)ptr);
  // if we didn't move it down in the address space, give up
  if ((char *)newptr > (char *)ptr) {
    je_dallocx(newptr, MALLOCX_TCACHE_NONE);
    return ptr;
  }
  memcpy(newptr, ptr, size);
  je_dallocx(ptr, MALLOCX_TCACHE_NONE);
  ent->ptr = newptr;
  return newptr;
}

void *halloc(size_t sz) {
  assert(sz < (1LLU << 32));
  alaska_mapping_t *ent = alaska_table_get();

  if (ent == NULL) {
    fprintf(stderr, "alaska: out of space!\n");
    exit(-1);
  }

  ent->size = sz;
  ent->ptr = je_calloc(1, sz);  // just use malloc
  // ent->ptr = bump_calloc(1, sz);

  // do some tracking
  halloc_calls++;
  update_memory_used(sz);

#ifdef ALASKA_SIM_MODE
  // record, then return the pointer itself
  extern void sim_on_alloc(alaska_mapping_t *);
  sim_on_alloc(ent);
  return ent->ptr;
#else
  uint64_t handle = HANDLE_MARKER | (((uint64_t)ent) << 32);
  return (void *)handle;
#endif
}

void *hcalloc(size_t nmemb, size_t size) { return halloc(nmemb * size); }

// Reallocate a handle
void *hrealloc(void *handle, size_t new_size) {
  if (handle == NULL) return halloc(new_size);

  alaska_mapping_t *m = alaska_lookup(handle);
  if (m == NULL) {
    return realloc(handle, new_size);
  }

  long old_size = m->size;
  void *new_ptr = je_realloc(m->ptr, new_size);

#ifdef ALASKA_SIM_MODE
  // record, then return the pointer itself
  extern void sim_on_realloc(alaska_mapping_t *, void *newptr, size_t newsz);
  sim_on_realloc(m, new_ptr, new_size);
#endif

  m->ptr = new_ptr;
  m->size = new_size;

  // do some tracking
  hrealloc_calls++;
  update_memory_used(-old_size + new_size);
#ifdef ALASKA_SIM_MODE
  // record, then return the pointer itself
  return m->ptr;
#else
  // realloc in alaska will always return the same pointer...
  // Ahh the benefits of using handles!
  // hopefully, nobody is relying on the output of realloc changing :^)
  return handle;
#endif
}

void hfree(void *ptr) {
  if (ptr == NULL) return;

  alaska_mapping_t *m = alaska_lookup(ptr);
  if (m == NULL) {
    return;
  }

#ifdef ALASKA_SIM_MODE
	extern void sim_on_free(alaska_mapping_t *);
	sim_on_free(m);
#endif

  hfree_calls++;

  long size = m->size;
  update_memory_used(-size);

  je_free(m->ptr);
  // bump_free(ent->ptr);

  // return the mapping to the table
  alaska_table_put(m);
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
