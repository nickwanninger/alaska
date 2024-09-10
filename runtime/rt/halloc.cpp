/*
 * This file is part of the Alaska Handle-Based Memory Management System
 *
 * Copyright (c) 2024, Nick Wanninger <ncw@u.northwestern.edu>
 * Copyright (c) 2024, The Constellation Project
 * All rights reserved.
 *
 * This is free software.  You are permitted to use, redistribute,
 * and modify it as specified in the file "LICENSE".
 */


#include <alaska/Runtime.hpp>
#include <alaska/ThreadCache.hpp>
#include <alaska.h>
#include <errno.h>


// TODO: don't have this be global!
static __thread alaska::ThreadCache *g_tc = nullptr;

alaska::ThreadCache *get_tc(void) {
  if (unlikely(g_tc == nullptr)) {
    g_tc = alaska::Runtime::get().new_threadcache();
  }
  return g_tc;
}




static void *_halloc(size_t sz, int zero) {
  void *result = get_tc()->halloc(sz, zero);

  // This seems right...
  if (result == NULL) errno = ENOMEM;
  return result;
}


void *halloc(size_t sz) noexcept { return _halloc(sz, 0); }

void *hcalloc(size_t nmemb, size_t size) { return _halloc(nmemb * size, 1); }

// Reallocate a handle
void *hrealloc(void *handle, size_t new_size) {
  auto *tc = get_tc();
  // If the handle is null, then this call is equivalent to malloc(size)
  if (handle == NULL) {
    return halloc(new_size);
  }
  auto *m = alaska::Mapping::from_handle_safe(handle);
  if (m == NULL) {
    if (!alaska::Runtime::get().heap.huge_allocator.owns(handle)) {
      log_debug("realloc edge case: not a handle %p!", handle);
      return ::realloc(handle, new_size);
    }
  }

  // If the size is equal to zero, and the handle is not null, realloc acts like free(handle)
  if (new_size == 0) {
    log_debug("realloc edge case: zero size %p!", handle);
    // If it wasn't a handle, just forward to the system realloc
    hfree(handle);
    return NULL;
  }

  handle = tc->hrealloc(handle, new_size);
  return handle;
}



void hfree(void *ptr) {
  // no-op if NULL is passed
  if (unlikely(ptr == NULL)) return;

  // Simply ask the thread cache to free it!
  get_tc()->hfree(ptr);
}




size_t alaska_usable_size(void *ptr) { return get_tc()->get_size(ptr); }
