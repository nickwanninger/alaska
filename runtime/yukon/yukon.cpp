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

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <alaska/core/ThreadCache.hpp>
#include <alaska/core/Runtime.hpp>
#include <assert.h>
#include <sys/signal.h>

#define PUBLIC __attribute__((visibility("default")))


#include "./shared.h"


static bool enable_printing = false;
extern "C" PUBLIC void yukon_enable_printing(int enable) { enable_printing = enable; }


static bool localization_blocked_by_environment = false;

extern "C" PUBLIC void yukon_enable_localization(int enable) {
  enable = 0;
  if (localization_blocked_by_environment) {
    fprintf(stderr, "YUKON: localization disabled by NODUMP env var!\n");
    enable = 0;
  }

  // the_runtime->with_barrier([&]() {
  //   alaska::printf("YUKON: Grading the heap...\n");
  //   the_runtime->grade_heap();
  //   alaska::printf("YUKON: Done grading the heap.\n");
  // });

  // -------------------------------------------- //
  // if (enable_localization && !enable) {
  //   // the_runtime->heap.compact_sizedpages();
  //   exit(0);
  //   return;
  // }
  // ------------------------------------------- //


  enable_localization = enable;
  alaska::printf("YUKON: localization %s\n", enable ? "enabled" : "disabled");
  if (enable_localization) {
    schedule_localization_interrupt();
  }
}



// Signal handler for segmentation faults
static void yukon_segfault_handler(int sig, siginfo_t *si, void *uc) {
  // Print the address that caused the segmentation fault
  alaska::printf("YUKON: fault %d at address: %p\n", sig, si->si_addr);
  // Print the instruction pointer at the time of the fault (riscv)
  ucontext_t *context = (ucontext_t *)uc;
#if defined(__riscv_xlen) && __riscv_xlen == 64
  alaska::printf("YUKON: Instruction pointer: %p\n", (void *)context->uc_mcontext.__gregs[REG_PC]);
#elif defined(__riscv_xlen) && __riscv_xlen == 32
  alaska::printf("YUKON: Instruction pointer: %p\n", (void *)context->uc_mcontext.__gregs[REG_PC]);
#else
  alaska::printf("YUKON: Instruction pointer: (unknown architecture)\n");
#endif


  char buffer[256];
  snprintf(buffer, sizeof(buffer), "cat /proc/%d/maps", getpid());
  setenv("LD_PRELOAD", "", 1);  // Unset LD_PRELOAD to avoid recursive faults in the handler.
  system(buffer);

  exit(-1);
}



static void CONSTRUCTOR yukon_init(void) {
  // Register segmentation fault handler
  struct sigaction sa;
  sa.sa_sigaction = yukon_segfault_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_SIGINFO;
  sigaction(SIGSEGV, &sa, NULL);
  sigaction(SIGILL, &sa, NULL);
  return;
  // Here, we initialize the dumping system in yukon.

  localization_blocked_by_environment = getenv("NODUMP") != nullptr;
  // Program the signal handler.


  alaska::ThreadCache::current();  // Force the runtime to initialize before we start getting
                                   // signals.

  signal(SIGPROF, yukon_dump_alarm_handler);

  if (getenv("LOCON") != nullptr) {
    alaska::printf("LOCON passed. turning localization on at boot.\n");
    yukon_enable_localization(true);
  } else {
    yukon_enable_localization(false);
  }



#if !defined(ALASKA_YUKON_NO_HARDWARE)
  if (getenv("YUKON_PHYS") != NULL) {
    alaska::printf("Setting up handles to bypass the TLB when they're cached!\n");
    uint64_t value;
    read_csr(CSR_HTBASE, value);
    alaska::printf("  HTBASE was 0x%lx\n", value);

    value |= (1LU << 63);
    alaska::printf("  Setting HTBASE to 0x%lx\n", value);
    write_csr(CSR_HTBASE, value);

    read_csr(CSR_HTBASE, value);
    alaska::printf("  Reading it back gave 0x%lx\n", value);
  }
#endif
}




// -------------------------------------------------------------- //
//                        Libc Overrides                          //
// -------------------------------------------------------------- //

PUBLIC void *operator new(size_t size) { return malloc(size); }
PUBLIC void *operator new[](size_t size) { return malloc(size); }
PUBLIC void operator delete(void *ptr) { free(ptr); }
PUBLIC void operator delete(void *ptr, unsigned long) { free(ptr); }
PUBLIC void operator delete[](void *ptr) { free(ptr); }


extern "C" {
PUBLIC void *malloc(size_t size) { return alaska::halloc(size); }
PUBLIC void *calloc(size_t size, size_t count) { return alaska::hcalloc(size, count); }
PUBLIC void *realloc(void *ptr, size_t newsize) { return alaska::hrealloc(ptr, newsize); }
PUBLIC void free(void *ptr) { return alaska::hfree(ptr); }
PUBLIC size_t malloc_usable_size(void *ptr) { return alaska::halloc_usable_size(ptr); }
}



static long seen = 0;

static long localize_structure_impl(alaska::Mapping *m, int depth, alaska::ThreadCache &tc) {
  long localized = 0;
  seen++;


  auto header = alaska::ObjectHeader::from(m);
  uint64_t *start = (uint64_t *)m->get_pointer();
  uint64_t *end = (uint64_t *)((char *)start + header->object_size());

  if (!header->localized) {
    localized += (long)tc.localize(m, 0);
  }

  if (depth == 0) return localized;

  header = alaska::ObjectHeader::from(m);

  header->walk([&](alaska::Mapping *om, alaska::ObjectHeader *oheader) {
    localized += localize_structure_impl(om, depth - 1, tc);
  });
  return localized;
}

extern "C" PUBLIC bool localize_structure(void *ptr) {
  auto &rt = alaska::Runtime::get();


  auto *m = alaska::Mapping::from_handle_safe(ptr);
  if (m == nullptr) return false;

  alaska::printf("Localizing structure at handle %p\n", ptr);
  return rt.with_barrier([&]() {
    auto *tc = alaska::ThreadCache::current();
    seen = 0;

    constexpr int loc_depth = 400;
    auto localize_count = localize_structure_impl(m, loc_depth, *tc);
  });
}
