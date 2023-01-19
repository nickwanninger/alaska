#include <alaska.h>
#include <alaska/internal.h>
#include <assert.h>
#include <jemalloc/jemalloc.h>
#include <malloc.h>
#include <string.h>
#include <sys/mman.h>

#if 0
#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)

// Maps 2*sz virtual address space, the upper half as a sentinel value.
static inline void *mmap_(size_t sz) {
  void *res = mmap(NULL, 2 * sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (res == MAP_FAILED) return NULL;
  mprotect((char *)res + sz, sz, PROT_NONE);
  return res;
}

const int SIG = 0xdead;
const size_t ALIGN = 32;
const size_t KB = 1024, MB = KB * KB, TB = MB * MB;
const size_t MAX_SIZE = 1 * TB;
static char *buf_start;
static char *bump;


#define aligned(in, align) (((uintptr_t)in + align - 1) & -align)
#define safe_mul(dst, src) (dst) *= (src)

typedef struct {
  uint32_t real_size;
  uint32_t handle;
  int sig;
} header_t;

// calculate aligned header size
#define HEADER_SIZE aligned(sizeof(header_t), ALIGN)

void bump_free(void *);
void *bump_malloc(size_t sz) {
  size_t real_size = ((sz > 2048) ? sz * 3 : sz) + HEADER_SIZE;  // expect realloc ahead of time!!!

  sz = aligned(real_size, ALIGN);

  // Assume that if this returns null we're being initialized and this can
  // only happen in one thread. (i.e. we assume that malloc must be called
  // before the first thread is created. using clone directly it might be
  // possible to avoid malloc until then, but let's assume you don't.)
  char *res = bump;  // TODO: atomic
  bump += sz;
  if (unlikely(res == NULL)) {
    size_t max_size = MAX_SIZE;
    while (!(buf_start = (char *)mmap_(max_size)))
      if (!(max_size >>= 1)) abort();
    bump = buf_start + sz;  // TODO: atomic
    res = buf_start;
  }

  ((header_t *)res)->real_size = sz - HEADER_SIZE;
  ((header_t *)res)->sig = SIG;
  res = res + HEADER_SIZE;

  return (char *)res;
}

void *bump_calloc(size_t n, size_t sz) {
  if (safe_mul(sz, n)) {
    void *p = bump_malloc(sz);
    memset(p, 0, sz);  // zeroing is necessary as free will move bump pointer back!!!
    return p;
  }
inline header_t *block2header(void *p) { return (header_t *)((char *)p - HEADER_SIZE); }

void *bump_realloc(void *p, size_t sz) {
  if (unlikely(sz <= 0)) {
    if (p != NULL) bump_free(p);
    return NULL;
  } else if (unlikely(p == NULL)) {
    return bump_malloc(sz);
  } else {
    header_t *h = block2header(p);
    if (unlikely(h->sig != SIG)) {
      abort();
    }
    if (h->real_size >= sz) return p;

    // try to expand last block by just bumping pointer
    size_t new_sz = aligned(sz + HEADER_SIZE, ALIGN) - HEADER_SIZE;  // apply coefficient as well???
    char *np = (char *)p + h->real_size;
    char *next = (char *)p + new_sz;
    if (__sync_bool_compare_and_swap(&bump, np, next)) {
      // correct real_size
      h->real_size = new_sz;
      return p;
    }

    // If sz is expanding the buffer, we may read outside it. No matter though,
    // that's going to be mapped (to zero pages if nothing else) unless sz is
    // large enough to reach the end of memory.
    return memcpy(bump_malloc(sz), p, sz);
  }
}

inline void bump_free(void *p) {
  if (unlikely(p == NULL)) return;

  header_t *h = block2header(p);
  if (unlikely(h->sig != SIG)) {
    abort();
  }
  h->sig = 0;
  char *np = (char *)p + h->real_size;
  // try freeing last block => improves memory usage and cache locality
  __sync_bool_compare_and_swap(&bump, np, (char *)h);
}
  return NULL;
}

inline header_t *block2header(void *p) { return (header_t *)((char *)p - HEADER_SIZE); }

void *bump_realloc(void *p, size_t sz) {
  if (unlikely(sz <= 0)) {
    if (p != NULL) bump_free(p);
    return NULL;
  } else if (unlikely(p == NULL)) {
    return bump_malloc(sz);
  } else {
    header_t *h = block2header(p);
    if (unlikely(h->sig != SIG)) {
      abort();
    }
    if (h->real_size >= sz) return p;

    // try to expand last block by just bumping pointer
    size_t new_sz = aligned(sz + HEADER_SIZE, ALIGN) - HEADER_SIZE;  // apply coefficient as well???
    char *np = (char *)p + h->real_size;
    char *next = (char *)p + new_sz;
    if (__sync_bool_compare_and_swap(&bump, np, next)) {
      // correct real_size
      h->real_size = new_sz;
      return p;
    }

    // If sz is expanding the buffer, we may read outside it. No matter though,
    // that's going to be mapped (to zero pages if nothing else) unless sz is
    // large enough to reach the end of memory.
    return memcpy(bump_malloc(sz), p, sz);
  }
}

inline void bump_free(void *p) {
  if (unlikely(p == NULL)) return;

  header_t *h = block2header(p);
  if (unlikely(h->sig != SIG)) {
    abort();
  }
  h->sig = 0;
  char *np = (char *)p + h->real_size;
  // try freeing last block => improves memory usage and cache locality
  __sync_bool_compare_and_swap(&bump, np, (char *)h);
}
#endif



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
  // void *newptr = bump_malloc(ent->size);
  // if (newptr > ent->ptr) {
  //   bump_free(newptr);
  //   return ent->ptr;
  // }
  //
  // memcpy(newptr, ent->ptr, ent->size);
  // bump_free(ent->ptr);
  // ent->ptr = newptr;
  // return ent->ptr;

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
  uint64_t handle = HANDLE_MARKER | (((uint64_t)ent) << 32);

  if (ent == NULL) {
    fprintf(stderr, "alaska: out of space!\n");
    exit(-1);
  }

  ent->size = sz;
  ent->ptr = je_calloc(1, sz);  // just use malloc
  // ent->ptr = bump_calloc(1, sz);


#ifdef ALASKA_CLASS_TRACKING
  ent->object_class = 0;
#endif

  // do some tracking
  halloc_calls++;
  update_memory_used(sz);

  return (void *)handle;
}

void *hcalloc(size_t nmemb, size_t size) { return halloc(nmemb * size); }

// Reallocate a handle
void *hrealloc(void *handle, size_t new_size) {
  if (handle == NULL) return halloc(new_size);
  uint64_t h = (uint64_t)handle;

  if (unlikely((h & HANDLE_MARKER) == 0)) {
    return realloc(handle, new_size);
  }

  alaska_mapping_t *ent = GET_ENTRY(handle);
  long old_size = ent->size;
  ent->ptr = je_realloc(ent->ptr, new_size);
  // ent->ptr = bump_realloc(ent->ptr, new_size);
  ent->size = new_size;

  // do some tracking
  hrealloc_calls++;
  update_memory_used(-old_size + new_size);

  // realloc in alaska will always return the same pointer...
  // Ahh the benefits of using handles!
  // hopefully, nobody is relying on the output of realloc changing :^)
  return handle;
}

void hfree(void *ptr) {
  if (ptr == NULL) return;

  uint64_t h = (uint64_t)ptr;
  if (unlikely((h & HANDLE_MARKER) == 0)) {
    return free(ptr);
  }

  hfree_calls++;
  alaska_mapping_t *ent = GET_ENTRY(ptr);
  long size = ent->size;
  update_memory_used(-size);

  je_free(ent->ptr);
  // bump_free(ent->ptr);
  
	// return the mapping to the table
  alaska_table_put(ent);
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
