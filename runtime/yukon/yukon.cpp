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


#include <dlfcn.h>
#include <alaska/alaska.hpp>
#include <alaska/utils.h>
#include <alaska/Runtime.hpp>
#include "alaska/liballoc.h"
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <execinfo.h>
#include <assert.h>

#ifdef __riscv
#define write_csr(reg, val) \
  ({ asm volatile("csrw " #reg ", %0" ::"rK"((uint64_t)val) : "memory"); })
#else
#define write_csr(reg, val)
// #warning YUKON expects riscv
#endif

static alaska::ThreadCache *tc = NULL;

static void set_ht_addr(void *addr) {
  printf("set htaddr to %p\n", addr);
  // write_csr(0xc2, addr);
}

static alaska::Runtime *the_runtime = NULL;


#include <execinfo.h>
#include <unistd.h>

#define BACKTRACE_SIZE 100

static bool dead = false;


static void print_hex(const char *msg, uint64_t val) {
  char buf[512];
  snprintf(buf, 512, "%s 0x%zx\n", msg, val);
  write(STDOUT_FILENO, buf, strlen(buf));
}

static void print_words(unsigned *words, int n) {
  char buf[512];
  for (int i = 0; i < n; i++) {
    snprintf(buf, 512, "%08x \n", words[i]);
    write(STDOUT_FILENO, buf, strlen(buf));
  }
  write(STDOUT_FILENO, "\n", 1);
}

void segfault_handler(int sig) {
  dead = true;
  write(STDOUT_FILENO, "segfault\n", 4);
  void *array[BACKTRACE_SIZE];
  size_t size;

  // Get the backtrace
  size = backtrace(array, BACKTRACE_SIZE);

  for (size_t i = 0; i < size; i++) {
    print_hex("bt:", (uint64_t)array[i]);
    print_words((unsigned*)array[i], 4);
  }
  exit(0);

  // // Print the backtrace to stderr
  char **ns = backtrace_symbols(array, size);

  // for (int i = 0; i < size; i++) {
  //   write_int(
  // }

}

static void init(void) {
  // signal(SIGSEGV, segfault_handler);
  the_runtime = new alaska::Runtime();
  void *handle_table_base = the_runtime->handle_table.get_base();
  printf("Handle table at %p\n", handle_table_base);
  set_ht_addr(handle_table_base);
  // Make sure the handle table performs mlocks
  the_runtime->handle_table.enable_mlock();
}




static alaska::ThreadCache *get_tc() {
  if (the_runtime == NULL) {
    init();
  }
  if (tc == NULL) {
    tc = the_runtime->new_threadcache();
  }
  return tc;
}

void __attribute__((constructor(102))) alaska_init(void) {}

void __attribute__((destructor)) alaska_deinit(void) {
  if (the_runtime != NULL) {
    if (tc != NULL) {
      the_runtime->del_threadcache(tc);
    }
    delete the_runtime;
  }
  set_ht_addr(NULL);
}



static void *_halloc(size_t sz, int zero) {
  print_hex("_halloc", sz);
  if (dead) {
    return alaska_internal_malloc(sz);
  }
  void *result = get_tc()->halloc(sz, zero);

  // This seems right...
  if (result == NULL) errno = ENOMEM;
  return result;
}

#define malloc halloc
#define calloc hcalloc
#define realloc hrealloc
#define free hfree

extern "C" void *malloc(size_t sz) noexcept { return _halloc(sz, 0); }

extern "C" void *calloc(size_t nmemb, size_t size) { return _halloc(nmemb * size, 1); }

// Reallocate a handle
extern "C" void *realloc(void *handle, size_t new_size) {
  auto *tc = get_tc();

  print_hex("realloc", (uint64_t)handle);
  print_hex("  newsz", (uint64_t)new_size);
  // If the handle is null, then this call is equivalent to malloc(size)
  if (handle == NULL) {
    return malloc(new_size);
  }
  auto *m = alaska::Mapping::from_handle_safe(handle);
  if (m == NULL) {
    if (!alaska::Runtime::get().heap.huge_allocator.owns(handle)) {
      log_debug("realloc edge case: not a handle %p!", handle);
      exit(-1);
      // return ::realloc(handle, new_size);
    }
  }

  // If the size is equal to zero, and the handle is not null, realloc acts like free(handle)
  if (new_size == 0) {
    log_debug("realloc edge case: zero size %p!", handle);
    // If it wasn't a handle, just forward to the system realloc
    free(handle);
    return NULL;
  }

  handle = tc->hrealloc(handle, new_size);
  return handle;
}



extern "C" void free(void *ptr) {
  print_hex("free", (uint64_t)ptr);
  // no-op if NULL is passed
  if (unlikely(ptr == NULL)) return;

  // Simply ask the thread cache to free it!
  get_tc()->hfree(ptr);
}
