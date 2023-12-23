#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>

static uint64_t timestamp() {
  struct timespec spec;
  clock_gettime(1, &spec);
  return spec.tv_sec * (1000 * 1000 * 1000) + spec.tv_nsec;
}

static long current_rss() {
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

static pthread_t rsstrack_thread;
static uint64_t interval_ms = 150;

static void *rsstrack_thread_func(void *arg) {

	FILE *out = fopen("rss.trace", "w");

  uint64_t thread_start_time = timestamp();
  while (1) {
		uint64_t now = timestamp();

		long rss = current_rss() * 1024;

		long t = (now - thread_start_time) / 1000 / 1000;

		fprintf(out, "%zu,%zu\n", t, rss);
		fflush(out);
    usleep(interval_ms * 1000);
  }

	fclose(out);
  return NULL;
}

void __attribute__((constructor(102))) rsstracker_init(void) {
  pthread_create(&rsstrack_thread, NULL, rsstrack_thread_func, NULL);
}
