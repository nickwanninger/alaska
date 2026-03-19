#pragma once
#include <stdlib.h>
#include <malloc.h>  // malloc_usable_size on glibc
static inline void *zmalloc(size_t sz)              { return malloc(sz); }
static inline void *zcalloc(size_t sz)              { return calloc(1, sz); }
static inline void *ztrycalloc(size_t sz)           { return calloc(1, sz); }
static inline void *zrealloc(void *p, size_t sz)    { return realloc(p, sz); }
static inline void  zfree(void *p)                  { free(p); }
static inline size_t zmalloc_size(void *p)          { return malloc_usable_size(p); }
#define zmalloc_usable_size(p) zmalloc_size(p)
static inline void *extend_to_usable(void *p, size_t sz) { (void)sz; return p; }
#define HAVE_MALLOC_SIZE 1
