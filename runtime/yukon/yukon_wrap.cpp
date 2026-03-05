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

// yukon_wrap: A hybrid runtime variant that uses Alaska's handle indirection
// (Mapping slab) but delegates actual data allocation to the system malloc.
// This isolates handle overhead from allocator overhead for benchmarking.

#define _GLIBCXX_INCLUDE_NEXT_C_HEADERS

#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <alaska/core/ThreadCache.hpp>
#include <alaska/core/Runtime.hpp>

#define CONSTRUCTOR __attribute__((constructor))

#define WRAP_DEBUG 1

#ifdef WRAP_DEBUG
#define wrap_debug(...) alaska::printf(__VA_ARGS__)
#else
#define wrap_debug(...) (void)0
#endif

static inline alaska::Runtime *the_runtime;

__attribute__((noinline)) static void yukon_init_runtime_and_tc() {
  the_runtime = new alaska::Runtime();
}

static inline alaska::ThreadCache *yukon_get_tc_unchecked() {
  return alaska::ThreadCache::current();
}

static inline alaska::ThreadCache *yukon_get_tc() {
  if (unlikely(the_runtime == nullptr)) yukon_init_runtime_and_tc();
  return yukon_get_tc_unchecked();
}


// ---------------------------------------------------------------- //
//                        Real libc functions                        //
// ---------------------------------------------------------------- //

using malloc_fn = void *(*)(size_t);
using free_fn = void (*)(void *);
using calloc_fn = void *(*)(size_t, size_t);
using realloc_fn = void *(*)(void *, size_t);
using malloc_usable_size_fn = size_t (*)(void *);

// Self-resolving trampolines: on first call they dlsym the real function,
// replace themselves, then tail-call. All subsequent calls go straight
// through the resolved pointer with zero branching.

static void *resolve_malloc(size_t);
static void resolve_free(void *);
static void *resolve_calloc(size_t, size_t);
static void *resolve_realloc(void *, size_t);
static size_t resolve_malloc_usable_size(void *);

static malloc_fn real_malloc = resolve_malloc;
static free_fn real_free = resolve_free;
static calloc_fn real_calloc = resolve_calloc;
static realloc_fn real_realloc = resolve_realloc;
static malloc_usable_size_fn real_malloc_usable_size = resolve_malloc_usable_size;

static void resolve_all() {
  wrap_debug("[wrap] resolve_all: dlsym malloc\n");
  real_malloc = (malloc_fn)dlsym(RTLD_NEXT, "malloc");
  wrap_debug("[wrap] resolve_all: dlsym free\n");
  real_free = (free_fn)dlsym(RTLD_NEXT, "free");
  wrap_debug("[wrap] resolve_all: dlsym calloc\n");
  real_calloc = (calloc_fn)dlsym(RTLD_NEXT, "calloc");
  wrap_debug("[wrap] resolve_all: dlsym realloc\n");
  real_realloc = (realloc_fn)dlsym(RTLD_NEXT, "realloc");
  wrap_debug("[wrap] resolve_all: dlsym malloc_usable_size\n");
  real_malloc_usable_size = (malloc_usable_size_fn)dlsym(RTLD_NEXT, "malloc_usable_size");
  wrap_debug("[wrap] resolve_all: done (malloc=%p free=%p calloc=%p)\n",
      (void*)real_malloc, (void*)real_free, (void*)real_calloc);
}

static void *resolve_malloc(size_t size) {
  wrap_debug("[wrap] trampoline: malloc(%zu)\n", size);
  resolve_all();
  return real_malloc(size);
}
static void resolve_free(void *ptr) {
  wrap_debug("[wrap] trampoline: free(%p)\n", ptr);
  resolve_all();
  real_free(ptr);
}
static void *resolve_calloc(size_t nmemb, size_t size) {
  wrap_debug("[wrap] trampoline: calloc(%zu, %zu)\n", nmemb, size);
  resolve_all();
  return real_calloc(nmemb, size);
}
static void *resolve_realloc(void *ptr, size_t size) {
  wrap_debug("[wrap] trampoline: realloc(%p, %zu)\n", ptr, size);
  resolve_all();
  return real_realloc(ptr, size);
}
static size_t resolve_malloc_usable_size(void *ptr) {
  wrap_debug("[wrap] trampoline: malloc_usable_size(%p)\n", ptr);
  resolve_all();
  return real_malloc_usable_size(ptr);
}


extern "C" void yukon_enable_localization(int enable) {}


// ---------------------------------------------------------------- //
//                      Allocation Interface                        //
// ---------------------------------------------------------------- //

static void *wrap_malloc(size_t size) {
  wrap_debug("[wrap] malloc(%zu)\n", size);
  auto *tc = yukon_get_tc();
  wrap_debug("[wrap] malloc: got tc=%p\n", (void*)tc);
  auto *m = tc->new_mapping();
  wrap_debug("[wrap] malloc: got mapping=%p\n", (void*)m);
  if (unlikely(m == nullptr)) return nullptr;

  void *data = real_malloc(size);
  wrap_debug("[wrap] malloc: real_malloc returned %p\n", data);
  if (unlikely(data == nullptr)) {
    tc->free_mapping(m);
    return nullptr;
  }

  m->set_pointer(data);
  void *handle = m->to_handle(0);
  wrap_debug("[wrap] malloc: returning handle=%p\n", handle);
  return handle;
}

static void wrap_free(void *ptr) {
  wrap_debug("[wrap] free(%p)\n", ptr);
  if (unlikely(ptr == nullptr)) return;

  if (!alaska::Mapping::is_handle(ptr)) {
    wrap_debug("[wrap] free: not a handle, forwarding to real_free\n");
    real_free(ptr);
    return;
  }

  auto *m = alaska::Mapping::from_handle(ptr);
  void *data = m->get_pointer();
  wrap_debug("[wrap] free: handle=%p mapping=%p data=%p\n", ptr, (void*)m, data);
  real_free(data);

  auto *tc = yukon_get_tc_unchecked();
  tc->free_mapping(m);
  wrap_debug("[wrap] free: done\n");
}

static void *wrap_realloc(void *ptr, size_t new_size) {
  if (ptr == nullptr) return wrap_malloc(new_size);

  if (new_size == 0) {
    wrap_free(ptr);
    return nullptr;
  }

  if (!alaska::Mapping::is_handle(ptr)) {
    return real_realloc(ptr, new_size);
  }

  auto *m = alaska::Mapping::from_handle(ptr);
  void *old_data = m->get_pointer();
  void *new_data = real_realloc(old_data, new_size);
  if (unlikely(new_data == nullptr)) return nullptr;

  m->set_pointer(new_data);
  // Handle stays stable -- the caller keeps the same handle.
  return ptr;
}

static void *wrap_calloc(size_t nmemb, size_t size) {
  wrap_debug("[wrap] calloc(%zu, %zu)\n", nmemb, size);
  auto *tc = yukon_get_tc();
  wrap_debug("[wrap] calloc: got tc=%p\n", (void*)tc);
  auto *m = tc->new_mapping();
  wrap_debug("[wrap] calloc: got mapping=%p\n", (void*)m);
  if (unlikely(m == nullptr)) return nullptr;

  void *data = real_calloc(nmemb, size);
  wrap_debug("[wrap] calloc: real_calloc returned %p\n", data);
  if (unlikely(data == nullptr)) {
    tc->free_mapping(m);
    return nullptr;
  }

  m->set_pointer(data);
  void *handle = m->to_handle(0);
  wrap_debug("[wrap] calloc: returning handle=%p\n", handle);
  return handle;
}

static size_t wrap_malloc_usable_size(void *ptr) {
  if (unlikely(ptr == nullptr)) return 0;

  if (!alaska::Mapping::is_handle(ptr)) {
    return real_malloc_usable_size(ptr);
  }

  auto *m = alaska::Mapping::from_handle(ptr);
  void *data = m->get_pointer();
  return real_malloc_usable_size(data);
}


// ---------------------------------------------------------------- //
//                    Libc / C++ operator overrides                  //
// ---------------------------------------------------------------- //

void *operator new(size_t size) { return wrap_malloc(size); }
void *operator new[](size_t size) { return wrap_malloc(size); }
void operator delete(void *ptr) { wrap_free(ptr); }
void operator delete(void *ptr, unsigned long) { wrap_free(ptr); }
void operator delete[](void *ptr) { wrap_free(ptr); }

extern "C" {
void *malloc(size_t size) { return wrap_malloc(size); }
void *calloc(size_t nmemb, size_t size) { return wrap_calloc(nmemb, size); }
void *realloc(void *ptr, size_t newsize) { return wrap_realloc(ptr, newsize); }
void free(void *ptr) { wrap_free(ptr); }
size_t malloc_usable_size(void *ptr) { return wrap_malloc_usable_size(ptr); }
}


// ---------------------------------------------------------------- //
//                         Initialization                           //
// ---------------------------------------------------------------- //

static char stdout_buf[BUFSIZ];
static char stderr_buf[BUFSIZ];

void CONSTRUCTOR alaska_init(void) {
  setvbuf(stdout, stdout_buf, _IOLBF, BUFSIZ);
  setvbuf(stderr, stderr_buf, _IOLBF, BUFSIZ);
  unsetenv("LD_PRELOAD");

  wrap_debug("[wrap] alaska_init: starting\n");
  resolve_all();
  wrap_debug("[wrap] alaska_init: resolve_all done, initializing TC\n");
  yukon_get_tc();
  wrap_debug("[wrap] alaska_init: done\n");
}
