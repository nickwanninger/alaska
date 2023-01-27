#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <malloc.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>

// Frack: they do that in alaska, right?


uint64_t timestamp() {
  struct timespec spec;
  clock_gettime(1, &spec);
  return spec.tv_sec * (1000 * 1000 * 1000) + spec.tv_nsec;
}


static size_t total_size = 0;

long alaska_get_rss_kb() {
#define bufLen 4096
  char buf[bufLen] = {0};

  int fd = open("/proc/self/status", O_RDONLY | O_CLOEXEC);
  if (fd < 0) return -1;

  ssize_t bytesRead = read(fd, buf, bufLen - 1);
  close(fd);

  if (bytesRead == -1) return -1;

  for (char *line = buf; line != NULL && *line != 0; line = strchr(line, '\n')) {
    if (*line == '\n') line++;
    if (strncmp(line, "VmRSS:", strlen("VmRSS:")) != 0) {
      continue;
    }

    char *rssString = line + strlen("VmRSS:");
    return atoi(rssString);
  }

  return -1;
}

static void report(void) {
  // ask malloc
  struct mallinfo2 info = mallinfo2();

  long sz = info.uordblks + info.fordblks;
  sz = alaska_get_rss_kb() * 1000.0;

  if (sz <= 0) return;
  float util = (float)total_size / (float)sz;

  if (util < 1.0) {
    // fprintf(stderr, "%zu,%f,%zu\n", timestamp(), util, sz);
    fprintf(stderr, "%zu,%zu,%zu\n", timestamp(), total_size, sz);
  }
}


static void *(*real_malloc)(size_t) = NULL;
static void *(*real_realloc)(void *, size_t) = NULL;
static size_t (*real_free)(void *) = NULL;

static void mtrace_init(void) {
  real_malloc = dlsym(RTLD_NEXT, "malloc");
  real_realloc = dlsym(RTLD_NEXT, "realloc");
  real_free = dlsym(RTLD_NEXT, "free");
  if (NULL == real_malloc) {
    fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
  }
}

void *malloc(size_t size) {
  if (real_malloc == NULL) mtrace_init();

  void *p = NULL;
  p = real_malloc(size);
  total_size += size;
  report();
  return p;
}

void *realloc(void *ptr, size_t size) {
  if (real_realloc == NULL) mtrace_init();
  if (ptr != 0) {
    size_t sz = malloc_usable_size(ptr);
    total_size -= sz;
  }

  void *p = real_realloc(ptr, size);
  total_size += size;
  if (size != 0) {
    report();
  }

  return p;
}

void free(void *ptr) {
  if (ptr == NULL) return;
  if (real_free == NULL) mtrace_init();

  size_t sz = malloc_usable_size(ptr);
  total_size -= sz;
  real_free(ptr);

  // report();
}