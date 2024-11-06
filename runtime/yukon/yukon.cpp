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

#include <yukon/yukon.hpp>


#define write_csr(reg, val) \
  ({ asm volatile("csrw %0, %1" ::"i"(reg), "rK"((uint64_t)val) : "memory"); })


#define read_csr(csr, val) \
  __asm__ __volatile__("csrr %0, %1" : "=r"(val) : "n"(csr) : /* clobbers: none */);



struct AutoFencer {
  ~AutoFencer() { __asm__ volatile("fence" ::: "memory"); }
};

#define wait_for_csr_zero(reg)         \
  do {                                 \
    volatile uint32_t csr_value = 0x1; \
    do {                               \
      read_csr(reg, csr_value);        \
    } while (csr_value != 0);          \
  } while (0);


#define CSR_HTBASE 0xc2
#define CSR_HTDUMP 0xc3
#define CSR_HTINVAL 0xc4



static void yukon_signal_handler(int sig, siginfo_t *info, void *ucontext) {
  AutoFencer fencer;
  // If a pagefault occurs while handle table walking, we will throw the
  // exception back up and even if you handle the page fault, the HTLB
  // stores the fact that it will cause an exception until you
  // invalidate the entry.
  if (sig == SIGSEGV) {
    // TODO: if the faulting address has the top bit set  (sv39) then we need to
    //       treat that as a page fault to the *handle table*. Basically, we need
    //       to read/write that handle entry.
    printf("Caught segfault to address %p. Clearing htlb and trying again!\n", info->si_addr);
    write_csr(CSR_HTINVAL, ((1LU << (64 - ALASKA_SIZE_BITS)) - 1));
    return;
  }


  // Pause requested.
  if (sig == SIGUSR2) {
    return;
  }
}


static void segv_handler(int sig, siginfo_t *info, void *ucontext) {}

static void setup_signal_handlers(void) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));

  // Block signals while we are in these signal handlers.
  sigemptyset(&sa.sa_mask);
  sigaddset(&sa.sa_mask, SIGUSR2);

  // Store siginfo (ucontext) on the stack of the signal
  // handlers (so we can grab the return address)
  sa.sa_flags = SA_SIGINFO;
  // Go to the `barrier_signal_handler`
  sa.sa_sigaction = yukon_signal_handler;
  // Attach this action on two signals:
  assert(sigaction(SIGSEGV, &sa, NULL) == 0);
  assert(sigaction(SIGUSR2, &sa, NULL) == 0);
}



#define ALASKA_THREAD_TRACK_STATE_T int
#define ALASKA_THREAD_TRACK_INIT setup_signal_handlers();
#include <alaska/thread_tracking.in.hpp>

static thread_local alaska::ThreadCache *tc = NULL;
static alaska::Runtime *the_runtime = NULL;


static inline uint64_t read_cycle_counter() {
  uint64_t cycles;
  asm volatile("rdcycle %0" : "=r"(cycles));
  return cycles;
}



static pthread_t yukon_dump_daemon_thread;
static void *yukon_dump_daemon(void *) {
  auto *tc = yukon::get_tc();
  while (1) {
    // Sleep

    // auto start = read_cycle_counter();
    usleep(10 * 1000);
    // auto end = read_cycle_counter();
    // printf("Slept for %lu cycles\n", end - start);
    // yukon::dump_htlb(tc);
    yukon::print_htlb();
  }

  return NULL;
}




namespace yukon {
  void set_handle_table_base(void *addr) {
    uint64_t value = (uint64_t)addr;
    if (value != 0 and getenv("YUKON_PHYS") != nullptr) {
      value |= (1LU << 63);
    }
    alaska::printf("set htbase to 0x%zx\n", value);
    write_csr(CSR_HTBASE, value);
  }




  void dump_htlb(alaska::ThreadCache *tc) {
    auto size = 576;
    auto *space = tc->localizer.get_hotness_buffer(size);
    memset(space, 0, size * sizeof(alaska::handle_id_t));

    asm volatile("fence" ::: "memory");

    auto start = read_cycle_counter();
    write_csr(CSR_HTDUMP, (uint64_t)space);
    wait_for_csr_zero(CSR_HTDUMP);
    auto end = read_cycle_counter();
    asm volatile("fence" ::: "memory");
    tc->localizer.feed_hotness_buffer(size, space);
    asm volatile("fence" ::: "memory");
    printf("Dumping htlb took %lu cycles\n", end - start);
  }


  void print_htlb(void) {
    size_t size = 576;
    uint64_t handle_ids[size];
    memset(handle_ids, 0, size * sizeof(alaska::handle_id_t));

    asm volatile("fence" ::: "memory");
    write_csr(CSR_HTDUMP, (uint64_t)handle_ids);
    wait_for_csr_zero(CSR_HTDUMP);
    asm volatile("fence" ::: "memory");

    printf("========================\n");

    int cols = 0;
    for (size_t i = 0; i < size; i++) {
      printf("%16lx ", handle_ids[i]);
      cols++;
      if (cols >= 8) {
        cols = 0;
        printf("\n");
      }
    }
    if (cols != 0) {
      printf("\n");
    }
  }

  alaska::ThreadCache *get_tc() {
    if (the_runtime == NULL) init();
    if (tc == NULL) tc = the_runtime->new_threadcache();
    return tc;
  }


  static char stdout_buf[BUFSIZ];
  static char stderr_buf[BUFSIZ];
  void init(void) {
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
    yukon::set_handle_table_base(handle_table_base);
    asm volatile("fence" ::: "memory");

    // pthread_create(&yukon_dump_daemon_thread, NULL, yukon_dump_daemon, NULL);
  }
}  // namespace yukon



void __attribute__((constructor(102))) alaska_init(void) {
  unsetenv("LD_PRELOAD");  // make it so we don't run alaska in subprocesses!
  yukon::get_tc();

  atexit([]() {
    printf("Setting ht addr to zero!\n");
    yukon::set_handle_table_base(NULL);
    printf("set!\n");
  });
}

void __attribute__((destructor)) alaska_deinit(void) {}

static void *_halloc(size_t sz, int zero) {
  // HACK: make it so we *always* zero the data to avoid pagefaults
  //       *this is slow*
  // zero = 1;
  void *result = NULL;

  result = yukon::get_tc()->halloc(sz, zero);
  auto m = (uintptr_t)alaska::Mapping::translate(result);
  if (result == NULL) errno = ENOMEM;

  return result;
}

extern "C" void *halloc(size_t sz) noexcept {
  AutoFencer fencer;
  return _halloc(sz, 0);
}
extern "C" void *hcalloc(size_t nmemb, size_t size) {
  AutoFencer fencer;
  return _halloc(nmemb * size, 1);
}

// Reallocate a handle
extern "C" void *hrealloc(void *handle, size_t new_size) {
  AutoFencer fencer;

  auto *tc = yukon::get_tc();

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
  AutoFencer fencer;
  // no-op if NULL is passed
  if (unlikely(ptr == NULL)) return;
  yukon::get_tc()->hfree(ptr);
}


extern "C" size_t halloc_usable_size(void *ptr) { return yukon::get_tc()->get_size(ptr); }



void *operator new(size_t size) { return halloc(size); }

void *operator new[](size_t size) { return halloc(size); }


void operator delete(void *ptr) { hfree(ptr); }

void operator delete[](void *ptr) { hfree(ptr); }
