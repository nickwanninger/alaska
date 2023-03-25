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
#include <alaska/internal.h>
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



struct alaska_pthread_trampoline_arg {
  void* arg;
  void* (*start)(void*);
};

void* alaska_pthread_trampoline(void* varg) {
  void* (*start)(void*);
  struct alaska_pthread_trampoline_arg* arg = varg;
  void* thread_arg = arg->arg;
  start = arg->start;
  free(arg);


  pthread_t self = pthread_self();
  alaska_barrier_add_thread(&self);
  void* ret = start(thread_arg);
  alaska_barrier_remove_thread(&self);

  return ret;
}

#undef pthread_create

int pthread_create(
    pthread_t* __restrict thread, const pthread_attr_t* __restrict attr, void* (*start)(void*), void* __restrict arg) {
  int rc;
  static int (*real_create)(pthread_t* __restrict thread, const pthread_attr_t* __restrict attr, void* (*start)(void*),
      void* __restrict arg) = NULL;
  if (!real_create) real_create = dlsym(RTLD_NEXT, "pthread_create");

  struct alaska_pthread_trampoline_arg* args = calloc(1, sizeof(struct alaska_pthread_trampoline_arg));
  args->arg = arg;
  args->start = start;
  rc = real_create(thread, attr, alaska_pthread_trampoline, args);
  return rc;
}


extern void alaska_service_init(void);
extern void alaska_service_deinit(void);

// High priority constructor: todo: do this lazily when you call halloc the first time.
void __attribute__((constructor(102))) alaska_init(void) {
  // add ourselves to the list of active threads.
  pthread_t self = pthread_self();
  alaska_barrier_add_thread(&self);

  alaska_table_init();
  alaska_halloc_init();
  alaska_service_init();

#ifdef ALASKA_CLASS_TRACKING
  alaska_classify_init();
#endif
}

void __attribute__((destructor)) alaska_deinit(void) {
  alaska_service_deinit();
  alaska_halloc_deinit();
  alaska_table_deinit();

  // Note: the main thread never removes itself from the barrier thread list

#ifdef ALASKA_CLASS_TRACKING
  alaska_classify_deinit();
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


uint64_t alaska_timestamp() {
#if defined(__x86_64__)
  uint32_t rdtsc_hi_, rdtsc_lo_;
  __asm__ volatile("rdtsc" : "=a"(rdtsc_lo_), "=d"(rdtsc_hi_));
  return (uint64_t)rdtsc_hi_ << 32 | rdtsc_lo_;
#else

  struct timespec spec;
  clock_gettime(1, &spec);
  return spec.tv_sec * (1000 * 1000 * 1000) + spec.tv_nsec;
#endif
}




void alaska_swap_in(alaska_mapping_t *m) {
	while(1) {}
  // printf("swap in %p\n", m);
}


// char* alaska_randomart(const unsigned char* raw, size_t raw_len, size_t fldbase, const char* palette) {
//   /*
// 	 * Chars to be used after each other every time the worm
// 	 * intersects with itself.  Matter of taste.
// 	 */
// 	const char *augmentation_string = " .o+=*BOX@%&#/^SE";
// 	if (palette != NULL) augmentation_string = palette;
// 	size_t len = strlen(augmentation_string) - 1;
//
// 	char	*retval, *p;
// 	size_t	b, i, fld_x, fld_y;
// 	ssize_t	x, y;
//
// 	/*
// 	* Field sizes for the random art.  Have to be odd, so the starting point
// 	* can be in the exact middle of the picture.
// 	*/
// 	fld_x = fldbase * 2 + 1;
// 	fld_y = fldbase + 1;
// 	/* make odd */
// 	if ((fldbase & 1) == 1) {
// 		fld_x -= 2;
// 		fld_y--;
// 	}
// 	unsigned char field[fld_x][fld_y];
//
// 	if ((retval = calloc(((size_t)fld_x + 3), ((size_t)fld_y + 2))) == NULL) {
// 		perror("ERROR calloc()");
// 		return NULL;
// 	}
//
// 	/* initialize field */
// 	memset(field, 0, fld_x * fld_y * sizeof(char));
// 	x = fld_x / 2;
// 	y = fld_y / 2;
//
// 	/* process user's input */
// 	for (i = 0; i < raw_len; i++) {
// 		int input;
// 		input = raw[i];
// 		/* each input byte conveys four 2-bit move commands */
// 		for (b = 0; b < 4; b++) {
// 			/* evaluate 2 bit, rest is shifted later */
// 			x += (input & 0x1) ? 1 : -1;
// 			y += (input & 0x2) ? 1 : -1;
//
// 			/* assure we are still in bounds */
// 			x = MAX(x, 0);
// 			y = MAX(y, 0);
// 			x = MIN(x, (ssize_t)fld_x - 1);
// 			y = MIN(y, (ssize_t)fld_y - 1);
//
// 			/* augment the field */
// 			if (field[x][y] < len - 2)
// 				field[x][y]++;
// 			input = input >> 2;
// 		}
// 	}
//
// 	/* mark starting point and end point*/
// 	field[fld_x / 2][fld_y / 2] = len - 1;
// 	field[x][y] = len;
//
// 	/* output upper border */
// 	p = retval;
// 	*p++ = '+';
// 	for (i = 0; i < fld_x / 2; i++)
// 		*p++ = '-';
// 	for (i = p - retval - 1; i < fld_x; i++)
// 		*p++ = '-';
// 	*p++ = '+';
// 	*p++ = '\n';
//
// 	/* output content */
// 	for (y = 0; y < (ssize_t)fld_y; y++) {
// 		*p++ = '|';
// 		for (x = 0; x < (ssize_t)fld_x; x++)
// 			*p++ = augmentation_string[MIN(field[x][y], len)];
// 		*p++ = '|';
// 		*p++ = '\n';
// 	}
//
// 	/* output lower border */
// 	*p++ = '+';
// 	for (i = 0; i < fld_x; i++)
// 		*p++ = '-';
// 	*p++ = '+';
//
// 	/* return art */
// 	return retval;
// }
