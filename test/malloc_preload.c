#define _GNU_SOURCE

#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>

static void *(*real_malloc)(size_t) = NULL;

static long alloc_count = 0;

static void mtrace_init(void) {
  real_malloc = dlsym(RTLD_NEXT, "malloc");
  if (NULL == real_malloc) {
		exit(-1);
  }
}

void *malloc(size_t size) {
  if (real_malloc == NULL) {
    mtrace_init();
		fprintf(stderr, "nr,size,address\n");
  }

  void *p = real_malloc(size);
  fprintf(stderr, "%ld,%zu,%p\n", alloc_count++, size, p);
  return p;
}
