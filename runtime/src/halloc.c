#include <alaska.h>
#include <alaska/internal.h>
#include <assert.h>
#include <jemalloc/jemalloc.h>
#include <malloc.h>
#include <string.h>
#include <sys/mman.h>

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

