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
    // the_runtime->brute_force_localization(*yukon_get_tc());
    // the_runtime->heap.compact_sizedpages();
  }
}



// Signal handler for segmentation faults
static void yukon_segfault_handler(int sig, siginfo_t *si, void *uc) {
  alaska::printf("YUKON: Segmentation fault at address: %p\n", si->si_addr);
  exit(-1);
}



static void CONSTRUCTOR yukon_init(void) {
  // Here, we initialize the dumping system in yukon.

  localization_blocked_by_environment = getenv("NODUMP") != nullptr;
  // Program the signal handler.


  yukon_get_tc();

  signal(SIGPROF, yukon_dump_alarm_handler);

  if (getenv("LOCON") != nullptr) {
    alaska::printf("LOCON passed. turning localization on at boot.\n");
    yukon_enable_localization(true);
  } else {
    yukon_enable_localization(false);
  }

  // Register segmentation fault handler
  // struct sigaction sa;
  // sa.sa_sigaction = yukon_segfault_handler;
  // sigemptyset(&sa.sa_mask);
  // sa.sa_flags = SA_SIGINFO;
  // sigaction(SIGSEGV, &sa, NULL);


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
//                     Allocation Interface                       //
// -------------------------------------------------------------- //


static void *_halloc(size_t sz, int zero) {
  void *result = NULL;


  alaska::LockedThreadCache tc = *yukon_get_tc();
  result = tc->halloc(sz);
  if (zero) {
    // NOTE: we can just memset here, no need to software translate!
    memset(result, 0, sz);
  }


  // if (enable_printing) alaska::printf("HALLOC %p\n", result);
  return result;
}

extern "C" PUBLIC void *halloc(size_t sz) noexcept {
  INSTRUCTION_TRACKER(INSTCOUNT_MALLOC);
  // LocalizationLatch loc_latch;
  return _halloc(sz, 0);
}
extern "C" PUBLIC void *hcalloc(size_t nmemb, size_t size) {
  INSTRUCTION_TRACKER(INSTCOUNT_CALLOC);
  // LocalizationLatch loc_latch;
  return _halloc(nmemb * size, 1);
}

// Reallocate a handle
extern "C" PUBLIC void *hrealloc(void *ptr, size_t new_size) {
  // If the ptr is null, then this call is equivalent to malloc(size)
  if (ptr == NULL) {
    return halloc(new_size);
  }

  // If the size is equal to zero, and the ptr is not null, realloc acts like free(ptr)
  if (new_size == 0) {
    // If it wasn't a ptr, just forward to the system realloc
    hfree(ptr);
    return NULL;
  }

  INSTRUCTION_TRACKER(INSTCOUNT_REALLOC);
  // LocalizationLatch loc_latch;
  alaska::LockedThreadCache tc = *yukon_get_tc();
  // if (enable_printing) alaska::printf("REALLOC %p\n", ptr);
  return tc->hrealloc(ptr, new_size);
}



extern "C" PUBLIC void hfree(void *ptr) {
  INSTRUCTION_TRACKER(INSTCOUNT_FREE);
  // LocalizationLatch loc_latch;
  // AutoFencer fencer;
  // no-op if NULL is passed
  if (unlikely(ptr == NULL)) return;
  alaska::LockedThreadCache tc = *yukon_get_tc_unchecked();


  // if (enable_printing) alaska::printf("HFREE %p\n", ptr);
  tc->hfree(ptr);
}


extern "C" PUBLIC size_t halloc_usable_size(void *ptr) {
  INSTRUCTION_TRACKER(INSTCOUNT_GETSIZE);
  auto tc = yukon_get_tc_unchecked();
  return tc->get_size(ptr);
}



// -------------------------------------------------------------- //
//                        Libc Overrides                          //
// -------------------------------------------------------------- //

PUBLIC void *operator new(size_t size) { return halloc(size); }
PUBLIC void *operator new[](size_t size) { return halloc(size); }
PUBLIC void operator delete(void *ptr) { hfree(ptr); }
PUBLIC void operator delete[](void *ptr) { hfree(ptr); }


extern "C" {
PUBLIC void *malloc(size_t size) { return halloc(size); }
PUBLIC void *calloc(size_t size, size_t count) { return hcalloc(size, count); }
PUBLIC void *realloc(void *ptr, size_t newsize) { return hrealloc(ptr, newsize); }
PUBLIC void free(void *ptr) { hfree(ptr); }
PUBLIC size_t malloc_usable_size(void *ptr) { return halloc_usable_size(ptr); }
}

static long seen = 0;

static long localize_structure_impl(alaska::Mapping *m, int depth, alaska::ThreadCache &tc) {
  long localized = 0;
  seen++;


  auto header = alaska::ObjectHeader::from(m);
  uint64_t *start = (uint64_t *)m->get_pointer();
  uint64_t *end = (uint64_t *)((char *)start + header->object_size());

  // alaska::printf("%6ld ", seen);
  // for (int i = 0; i < depth - 1; i++) alaska::printf("|  ");
  // alaska::printf("|--");
  // alaska::printf("%p,%p %zu | ", m, m->get_pointer(), header->object_size());
  // // gray
  // alaska::printf("\e[90m");
  // for (uint64_t *p = start; p < end; p++) {
  //   alaska::printf("%016lx ", *p);
  // }
  // alaska::printf("\e[0m\n");

  // if (depth > 2000) return localized;

  // then, walk its data

  if (!header->localized) {
    localized += (long)tc.localize(m, 0);
  }

  if (depth == 0) return localized;

  header = alaska::ObjectHeader::from(m);

  // header->walk([&](alaska::Mapping *om, alaska::ObjectHeader *oheader) {
  //   if (oheader->localized) return;
  //   localized += (long)tc.localize(om, 0);
  // });

  header->walk([&](alaska::Mapping *om, alaska::ObjectHeader *oheader) {
    localized += localize_structure_impl(om, depth - 1, tc);
  });
  return localized;
}

extern "C" bool localize_structure(void *ptr) {
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
