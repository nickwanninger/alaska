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
#include <ck/vec.h>

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
// #undef pthread_create
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

#include <alaska/StackMapParser.h>

const char* regname(int r) {
  switch (r) {
    case 0:
      return "RAX";
    case 1:
      return "RDX";
    case 2:
      return "RCX";
    case 3:
      return "RBX";
    case 4:
      return "RSI";
    case 5:
      return "RDI";
    case 6:
      return "RBP";
    case 7:
      return "RSP";
    case 8:
      return "R8";
    case 9:
      return "R9";
    case 10:
      return "R10";
    case 11:
      return "R11";
    case 12:
      return "R12";
    case 13:
      return "R13";
    case 14:
      return "R14";
    case 15:
      return "R15";
    case 16:
      return "RIP";
    case 17:
      return "XMM0";
    case 33:
      return "ST0";
    case 34:
      return "ST1";
    case 35:
      return "ST2";
    case 36:
      return "ST3";
    case 37:
      return "ST4";
    case 38:
      return "ST5";
    case 39:
      return "ST6";
    case 40:
      return "ST7";
    case 49:
      return "EFLAGS";
    case 50:
      return "ES";
    case 51:
      return "CS";
    case 52:
      return "SS";
    case 53:
      return "DS";
    case 54:
      return "FS";
    case 55:
      return "GS";
    case 62:
      return "TR";
    case 63:
      return "LDTR";
    case 64:
      return "MXCSR";
    case 65:
      return "CTRL";
    case 66:
      return "STAT";
  }
  return "UNK";
}


static alaska::StackMapParser* p;
void readStackMap(uint8_t* t) {
  p = new alaska::StackMapParser(t);

  printf("Version:     %d\n", p->getVersion());
  printf("Constants:   %d\n", p->getNumConstants());
  printf("Functions:   %d\n", p->getNumFunctions());

  int cur_record = 0;

  for (auto it = p->functions_begin(); it != p->functions_end(); it++) {
    auto& f = *it;
    // printf("Function addr:%p, sz:%zd, rec:%zd\n", f.getFunctionAddress(), f.getStackSize(),
    //     f.getRecordCount());

    for (uint64_t c = 0; c < f.getRecordCount(); c++) {
      auto r = p->getRecord(cur_record);
      cur_record++;

      off_t addr = f.getFunctionAddress() + r.getInstructionOffset();
      for (unsigned loc = 0; loc < r.getNumLocations(); loc++) {
        auto l = r.getLocation(loc);
        if (l.getKind() >= alaska::StackMapParser::LocationKind::Constant) {
          continue;
        }

        // auto sz = l.getSizeInBytes();
        auto dwarfRegNum = l.getDwarfRegNum();
        auto offset = l.getOffset();
        printf("%016p ", addr);
        switch (l.getKind()) {
          case alaska::StackMapParser::LocationKind::Register:
            printf("Register %d\n", dwarfRegNum);
            break;
          case alaska::StackMapParser::LocationKind::Direct:
            printf("Direct %s + %d\n", regname(dwarfRegNum), offset);
            break;
          case alaska::StackMapParser::LocationKind::Indirect:
            printf("Indirect %s + %d\n", regname(dwarfRegNum), offset);
            break;

          case alaska::StackMapParser::LocationKind::Constant:
          case alaska::StackMapParser::LocationKind::ConstantIndex:
            printf("<Constant>\n");
            break;
        }
      }
      for (uint64_t c = 0; c < r.getNumLiveOuts(); c++) {
        auto lo = r.getLiveOut(c);
        printf("%016p LiveOut %s", addr, regname(lo.getDwarfRegNum()));
      }
    }
  }
}

extern "C" void alaska_test_sm() {
  return;
  if (p == NULL) return;
  uint64_t retAddr = (uint64_t)__builtin_return_address(0);
  printf("Return address: %p\n", retAddr);

  int cur_record = 0;

  for (auto it = p->functions_begin(); it != p->functions_end(); it++) {
    auto& f = *it;
    for (uint64_t c = 0; c < f.getRecordCount(); c++) {
      auto r = p->getRecord(cur_record);
      cur_record++;
      uint64_t recAddr = f.getFunctionAddress() + r.getInstructionOffset();
      if (recAddr != retAddr) {
        continue;
      }
      printf("  Record %p locs:%d  (%p)\n", r.getInstructionOffset(), r.getNumLocations(),
          f.getFunctionAddress() + r.getInstructionOffset());
      for (unsigned loc = 0; loc < r.getNumLocations(); loc++) {
        auto l = r.getLocation(loc);
        printf("  - k:%d sz:%d reg:%d", l.getKind(), l.getSizeInBytes(), l.getDwarfRegNum());

        if (l.getKind() == alaska::StackMapParser::LocationKind::Direct ||
            l.getKind() == alaska::StackMapParser::LocationKind::Indirect) {
          printf(" offset:%d", l.getOffset());
        }
        if (l.getKind() == alaska::StackMapParser::LocationKind::ConstantIndex) {
          printf(" ci:%d", l.getConstantIndex());
        }
        printf("\n");
      }

      for (auto i = 0; i < r.getNumLiveOuts(); i++) {
        auto lo = r.getLiveOut(i);
        printf("  liveout - %d %d\n", lo.getDwarfRegNum(), lo.getSizeInBytes());
      }
    }
  }
}


extern "C" void alaska_register_stack_map(void* map) {
  if (map) {
    printf("Register stack map %p\n", map);
    readStackMap((uint8_t*)map);
  }
}
// High priority constructor: todo: do this lazily when you call halloc the first time.
void __attribute__((constructor(102))) alaska_init(void) {
  alaska::barrier::initialize_safepoint_page();

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


extern "C" void __cxa_pure_virtual(void) {
  abort();
}

extern "C" void* __alaska_leak(void* ptr) {
  return alaska_translate(ptr);
}
