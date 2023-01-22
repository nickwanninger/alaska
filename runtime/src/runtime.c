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


uint64_t alaska_next_usage_timestamp = 0;

// High priority constructor: todo: do this lazily when you call halloc the first time.
static void __attribute__((constructor(102))) alaska_init(void) {
  alaska_table_init();
  alaska_halloc_init();
#ifdef ALASKA_CLASS_TRACKING
  alaska_classify_init();
#endif
}

static void __attribute__((destructor)) alaska_deinit(void) {
#ifdef ALASKA_CLASS_TRACKING
  alaska_classify_deinit();
#endif
  alaska_halloc_deinit();
  alaska_table_deinit();
}

#define BT_BUF_SIZE 100
void alaska_dump_backtrace(void) {
  int nptrs;

  void* buffer[BT_BUF_SIZE];
  char** strings;

  nptrs = backtrace(buffer, BT_BUF_SIZE);
  printf("Backtrace:\n", nptrs);

  strings = backtrace_symbols(buffer, nptrs);
  if (strings == NULL) {
    perror("backtrace_symbols");
    exit(EXIT_FAILURE);
  }

  for (int j = 0; j < nptrs; j++)
    printf("\x1b[92m%d\x1b[0m: %s\n", j, strings[j]);

  free(strings);
}


long alaska_get_rss_kb() {
#if defined(__linux__)
#define bufLen 4096
  char buf[bufLen] = {0};

  int fd = open("/proc/self/status", O_RDONLY | O_CLOEXEC);
  if (fd < 0) return -1;

  ssize_t bytesRead = read(fd, buf, bufLen - 1);
  close(fd);

  if (bytesRead == -1) return -1;

  for (char* line = buf; line != NULL && *line != 0; line = strchr(line, '\n')) {
    if (*line == '\n') line++;
    if (strncmp(line, "VmRSS:", strlen("VmRSS:")) != 0) {
      continue;
    }

    char* rssString = line + strlen("VmRSS:");
    return atoi(rssString);
  }

  return -1;
#elif defined(__APPLE__)
  struct task_basic_info info;
  task_t curtask = MACH_PORT_NULL;
  mach_msg_type_number_t cnt = TASK_BASIC_INFO_COUNT;
  int pid = getpid();

  if (task_for_pid(current_task(), pid, &curtask) != KERN_SUCCESS) return -1;

  if (task_info(curtask, TASK_BASIC_INFO, (task_info_t)(&info), &cnt) != KERN_SUCCESS) return -1;

  return static_cast<int>(info.resident_size);
#else
  return 0;
#endif
}
