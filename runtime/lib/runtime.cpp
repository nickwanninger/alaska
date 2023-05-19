/*
 * This file is part of the Alaska Handle-Based Memory Management System
 *
 * Copyright (c) 2023, Nick Wanninger <ncw@u.northwestern.edu>
 * Copyright (c) 2023, The Constellation Project
 * All rights reserved.
 *
 * This is free software.  You are permitted to use, redistribute,
 * and modify it as specified in the file "LICENSE".
 */
#include <alaska.h>
#include <alaska/alaska.hpp>
#include <alaska/table.hpp>
#include <alaska/service.hpp>
#include <alaska/barrier.hpp>
#include "alaska/utils.h"

#include <assert.h>
#include <errno.h>
#include <execinfo.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <bits/pthreadtypes.h>
#include <dlfcn.h>
#include <pthread.h>

#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif



long alaska::translation_hits = 0;
long alaska::translation_misses = 0;

void alaska::record_translation_info(bool hit) {
	if (hit) {
		alaska::translation_hits++;
	} else {
		alaska::translation_misses++;
	}
}

struct alaska_pthread_trampoline_arg {
  void* arg;
  void* (*start)(void*);
};

static void* alaska_pthread_trampoline(void* varg) {
  void* (*start)(void*);
  auto* arg = (struct alaska_pthread_trampoline_arg*)varg;
  void* thread_arg = arg->arg;
  start = arg->start;
  free(arg);


  pthread_t self = pthread_self();
  alaska::barrier::add_thread(&self);
  void* ret = start(thread_arg);
  alaska::barrier::remove_thread(&self);

  return ret;
}


// Hook into thread creation by overriding the pthread_create function
#undef pthread_create
extern "C" int pthread_create(pthread_t* __restrict thread, const pthread_attr_t* __restrict attr,
    void* (*start)(void*), void* __restrict arg) {
  int rc;
  static int (*real_create)(pthread_t* __restrict thread, const pthread_attr_t* __restrict attr,
      void* (*start)(void*), void* __restrict arg) = NULL;
  if (!real_create) real_create = (decltype(real_create))dlsym(RTLD_NEXT, "pthread_create");

  auto* args = (struct alaska_pthread_trampoline_arg*)calloc(
      1, sizeof(struct alaska_pthread_trampoline_arg));
  args->arg = arg;
  args->start = start;
  rc = real_create(thread, attr, alaska_pthread_trampoline, args);
  return rc;
}


// High priority constructor: todo: do this lazily when you call halloc the first time.
void __attribute__((constructor(102))) alaska_init(void) {
  // Add the main thread to the list of active threads
  // Note: the main thread never removes itself from the barrier thread list
  pthread_t self = pthread_self();
  alaska::barrier::add_thread(&self);

  alaska::table::init();
  alaska::service::init();
}

void __attribute__((destructor)) alaska_deinit(void) {
  alaska::service::deinit();
  alaska::table::deinit();


#ifdef ALASKA_TRACK_TRANSLATION_HITRATE
  printf("[alaska] HITRATE INFORMATION:\n");
  printf("[alaska] hits:    %lu\n", alaska::translation_hits);
  printf("[alaska] misses:  %lu\n", alaska::translation_misses);
  printf("[alaska] hitrate: %5.2lf%%\n",
      100.0 * (alaska::translation_hits /
                  (float)(alaska::translation_misses + alaska::translation_hits)));
#endif
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


long alaska_translate_rss_kb() {
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




// Simply use clock_gettime, which is fast enough on most systems
uint64_t alaska_timestamp() {
  struct timespec spec;
  clock_gettime(1, &spec);
  return spec.tv_sec * (1000 * 1000 * 1000) + spec.tv_nsec;
}


void* alaska_ensure_present(alaska::Mapping* m) {
  // If the pointer is not null, we shouldn't do anything.
  if (m->ptr != NULL) {
    return m->ptr;
  }

  // Instead, if the pointer is null, we need to ask the service to swap it in!
  alaska::service::swap_in(m);

  ALASKA_SANITY(m->ptr != NULL, "Service did not swap in a handle");
  return m->ptr;
}
