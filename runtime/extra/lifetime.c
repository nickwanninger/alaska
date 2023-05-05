#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <dlfcn.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static FILE *lifetime_output = NULL;
static uint64_t start_timestamp = 0;
static volatile int do_trace = 0;


static uint64_t timestamp() {
  struct timespec spec;
  clock_gettime(1, &spec);
  return spec.tv_sec * (1000 * 1000 * 1000) + spec.tv_nsec;
}


// High priority constructor: todo: do this lazily when you call halloc the first time.
void __attribute__((constructor(102))) alaska_init(void) {
  char buf[512];
  sprintf(buf, "/tmp/raw_lifetime.%d", getpid());
  lifetime_output = fopen(buf, "w+");
  start_timestamp = timestamp();
  do_trace = 1;
}

void __attribute__((destructor)) alaska_deinit(void) {
  do_trace = 0;
  // postprocess the raw output into a normalized "percentage of runtime"
  uint64_t time_running = timestamp() - start_timestamp;
  fseek(lifetime_output, 0, SEEK_SET);

  char buf[512];
  sprintf(buf, "/tmp/lifetime.%d", getpid());
  FILE *norm_out = fopen(buf, "w+");

  size_t size, lifetime;


  char line[256];

  while (fgets(line, sizeof(line), lifetime_output)) {
    sscanf(line, "%zu,%zu\n", &size, &lifetime);
    double norm = (double)lifetime / (double)time_running;
    // fprintf(stderr, "> %zu,%lf  %zu %zu\n", size, norm, lifetime, time_running);
    fprintf(norm_out, "%zu,%lf\n", size, norm);
  }
  fclose(norm_out);
  fclose(lifetime_output);
}



typedef struct {
  uint64_t time_allocated_ns;
  uint32_t size;
  uint32_t magic;
} md_t;

#define MAGIC 0xCAFEF00D
static void *(*real_malloc)(size_t) = NULL;
static void *(*real_realloc)(void *, size_t) = NULL;
static void (*real_free)(void *) = NULL;

static void mtrace_init(void) {
  real_malloc = dlsym(RTLD_NEXT, "malloc");
  real_free = dlsym(RTLD_NEXT, "free");
  real_realloc = dlsym(RTLD_NEXT, "realloc");
}

void *malloc(size_t size) {
  if (real_malloc == NULL) {
    mtrace_init();
  }

  size_t size_with_overhead = size + sizeof(md_t);

  md_t *md = real_malloc(size_with_overhead);

  md->size = size;
  md->time_allocated_ns = timestamp();
  md->magic = MAGIC;

  return md + 1;
}


void *calloc(size_t size, size_t count) {
  void *p = malloc(size * count);
  memset(p, 0, size * count);
  return p;
}

void *realloc(void *p, size_t size) {
  if (real_realloc == NULL) {
    mtrace_init();
  }
  if (p == NULL) return malloc(size);

  md_t *m = (md_t *)p - 1;

  // printf("ra %p %zu\n", m, m->size);

  m = real_realloc(m, size + sizeof(md_t));
  m->size = size;

  return m + 1;
}


void free(void *p) {
  if (real_free == NULL) {
    mtrace_init();
  }
  if (p == NULL) return;


  if (do_trace) {
    md_t *m = (md_t *)p - 1;
    if (m->magic != MAGIC) return;
    ssize_t dur = timestamp() - m->time_allocated_ns;
    fprintf(lifetime_output, "%zu,%zd\n", m->size, dur);
  }

  // real_free(m);
}



size_t malloc_usable_size(void *p) {
  md_t *m = (md_t *)p - 1;
  return m->size;
}
