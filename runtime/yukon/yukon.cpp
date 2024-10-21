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
#include <alaska/Configuration.hpp>
#include "alaska/HugeObjectAllocator.hpp"
#include <alaska/liballoc.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <execinfo.h>
#include <assert.h>
#include <execinfo.h>
#include <unistd.h>

#ifdef __riscv
#define write_csr(reg, val) \
  ({ asm volatile("csrw " #reg ", %0" ::"rK"((uint64_t)val) : "memory"); })
#else
#define write_csr(reg, val)
// #warning YUKON expects riscv
#endif

static alaska::ThreadCache *tc = NULL;

static void set_ht_addr(void *addr) {
  alaska::printf("set htbase to %p\n", addr);
  uint64_t value = (uint64_t)addr;
  if (value != 0 and getenv("YUKON_PHYS") != nullptr) {
    value |= (1LU << 63);
  }
  write_csr(0xc2, value);
}

static alaska::Runtime *the_runtime = NULL;



#define BACKTRACE_SIZE 100

static bool dead = false;

static bool is_initialized() { return the_runtime != NULL; }


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


static void print_string(const char *msg) { write(STDOUT_FILENO, msg, strlen(msg)); }

static char stdout_buf[BUFSIZ];
static char stderr_buf[BUFSIZ];



static void wait_for_csr_zero(void) {
  volatile uint32_t csr_value = 0x1;
  do {
    __asm__ volatile("csrr %0, 0xc3\n\t" : "=r"(csr_value) : : "memory");
  } while (csr_value != 0);
}


static void dump_htlb() {
  auto &rt = alaska::Runtime::get();
  auto size = 576;
  auto *space = rt.locality_manager.get_hotness_buffer(size);
  memset(space, 0, size * sizeof(alaska::handle_id_t));

  asm volatile("fence" ::: "memory");

  write_csr(0xc3, (uint64_t)space);
  wait_for_csr_zero();
  asm volatile("fence" ::: "memory");

  rt.locality_manager.feed_hotness_buffer(size, space);
}

#define BACKTRACE_SIZE 100



static void handle_sig(int sig) {
  printf("Caught signal %d (%s)\n", sig, strsignal(sig));
  void *buffer[BACKTRACE_SIZE];
  // Get the backtrace
  int nptrs = backtrace(buffer, BACKTRACE_SIZE);
  // Print the backtrace symbols
  backtrace_symbols_fd(buffer, nptrs, STDOUT_FILENO);
  exit(0);
}


static void segv_handler(int sig, siginfo_t *info, void *ucontext) {
  printf("Caught segfault to address %p\n", info->si_addr);
  void *buffer[BACKTRACE_SIZE];
  // Get the backtrace
  int nptrs = backtrace(buffer, BACKTRACE_SIZE);
  // Print the backtrace symbols
  backtrace_symbols_fd(buffer, nptrs, STDOUT_FILENO);
  alaska_dump_backtrace();
  exit(0);
}


static void init(void) {
  setvbuf(stdout, stdout_buf, _IOLBF, BUFSIZ);
  setvbuf(stderr, stderr_buf, _IOLBF, BUFSIZ);

  alaska::Configuration config;
  // Use the "malloc" backend to operate cleanly w/ libc's malloc
  config.huge_strategy = alaska::HugeAllocationStrategy::CUSTOM_MMAP_BACKED;

  the_runtime = new alaska::Runtime(config);
  void *handle_table_base = the_runtime->handle_table.get_base();
  // printf("Handle table at %p\n", handle_table_base);
  // Make sure the handle table performs mlocks
  the_runtime->handle_table.enable_mlock();

  asm volatile("fence" ::: "memory");
  set_ht_addr(handle_table_base);

  struct sigaction act = {0};
  act.sa_sigaction = segv_handler;
  act.sa_flags = SA_SIGINFO;
  sigaction(SIGSEGV, &act, NULL);


  // signal(SIGABRT, handle_sig);
  // signal(SIGSEGV, handle_sig);
  // signal(SIGBUS, handle_sig);
  // signal(SIGILL, handle_sig);
}




static alaska::ThreadCache *get_tc() {
  if (the_runtime == NULL) init();
  if (tc == NULL) tc = the_runtime->new_threadcache();
  return tc;
}

void __attribute__((constructor(102))) alaska_init(void) {
  unsetenv("LD_PRELOAD");  // make it so we don't run alaska in subprocesses!
  get_tc();

  atexit([]() {
    printf("Setting ht addr to zero!\n");
    set_ht_addr(0);
    printf("set!\n");
  });
}

void __attribute__((destructor)) alaska_deinit(void) {}

static void *_halloc(size_t sz, int zero) {
  // HACK: make it so we *always* zero the data to avoid pagefaults
  //       *this is slow*
  zero = 1;
  void *result = NULL;

  result = get_tc()->halloc(sz, zero);
  auto m = (uintptr_t)alaska::Mapping::translate(result);
  if (result == NULL) errno = ENOMEM;

  // dump_htlb();


  return result;
}


// #define halloc malloc
// #define hcalloc calloc
// #define hrealloc realloc
// #define hfree free

extern "C" void *halloc(size_t sz) noexcept { return _halloc(sz, 0); }

extern "C" void *hcalloc(size_t nmemb, size_t size) { return _halloc(nmemb * size, 1); }

// Reallocate a handle
extern "C" void *hrealloc(void *handle, size_t new_size) {
  auto *tc = get_tc();

  // If the handle is null, then this call is equivalent to malloc(size)
  if (handle == NULL) {
    return malloc(new_size);
  }
  auto *m = alaska::Mapping::from_handle_safe(handle);
  if (m == NULL) {
    if (!alaska::Runtime::get().heap.huge_allocator.owns(handle)) {
      log_fatal("realloc edge case: not a handle %p!", handle);
      exit(-1);
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



extern "C" void hfree(void *ptr) {
  // no-op if NULL is passed
  if (unlikely(ptr == NULL)) return;

  // Simply ask the thread cache to free it!
  get_tc()->hfree(ptr);
}


extern "C" size_t halloc_usable_size(void *ptr) { return get_tc()->get_size(ptr); }



void *operator new(size_t size) { return halloc(size); }

void *operator new[](size_t size) { return halloc(size); }


void operator delete(void *ptr) { hfree(ptr); }

void operator delete[](void *ptr) { hfree(ptr); }
