#include <alaska/trace.h>
#include <alaska/internal.h>
#include <alaska.h>
#include <string.h>

#define TRACE(struct) fwrite(&struct, sizeof(struct), 1, get_trace_file())

static void close_trace_file(void);
static FILE *get_trace_file(void) {
  static FILE *stream = NULL;
  if (stream == NULL) {
    atexit(close_trace_file);
    stream = fopen("alaska.trace", "w");
  }
  return stream;
}

static void close_trace_file(void) { fclose(get_trace_file()); }

void *halloc_trace(size_t sz) {
  void *p = malloc(sz);
  struct alaska_trace_alloc t;
  t.type = 'A';
  t.ptr = (uint64_t)p;
  t.size = (uint64_t)sz;
  TRACE(t);

  return p;
}


void *hcalloc_trace(size_t nmemb, size_t size) {
  void *p = halloc_trace(nmemb * size);
  memset(p, 0, nmemb * size);
  return p;
}


void *hrealloc_trace(void *ptr, size_t new_size) {
  void *new_ptr = realloc(ptr, new_size);

  struct alaska_trace_realloc t;
  t.type = 'R';
  t.old_ptr = (uint64_t)ptr;
  t.new_ptr = (uint64_t)new_ptr;
  t.new_size = (uint64_t)new_size;
  TRACE(t);

  return new_ptr;
}


void hfree_trace(void *ptr) {
  struct alaska_trace_free t;
  t.type = 'F';
  t.ptr = (uint64_t)ptr;
  TRACE(t);

  free(ptr);
}



void *alaska_lock_trace(void *restrict ptr) {
  struct alaska_trace_lock t;
  t.type = 'L';
  t.ptr = (uint64_t)ptr;
  TRACE(t);

  return ptr;
}

void alaska_unlock_trace(void *restrict ptr) {
  struct alaska_trace_unlock t;
  t.type = 'U';
  t.ptr = (uint64_t)ptr;
  TRACE(t);
}
