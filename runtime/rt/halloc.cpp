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
#include "alaska/ThreadCache.hpp"
#include <alaska.h>
#include <errno.h>


// TODO: don't have this be global!
static alaska::ThreadCache *g_tc;

static auto get_tc(void) {
  if (g_tc == nullptr) g_tc = alaska::Runtime::get().new_threadcache();
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
  // If the handle is null, then this call is equivalent to malloc(size)
  if (handle == NULL) {
    // printf("realloc edge case: NULL pointer (sz=%zu)\n", new_size);
    return halloc(new_size);
  }
  auto *m = alaska::Mapping::from_handle_safe(handle);
  if (m == NULL) {
    log_debug("realloc edge case: not a handle %p!", handle);
    return ::realloc(handle, new_size);
  }

  // If the size is equal to zero, and the handle is not null, realloc acts like free(handle)
  if (new_size == 0) {
    log_debug("realloc edge case: zero size %p!", handle);
    // If it wasn't a handle, just forward to the system realloc
    hfree(handle);
    return NULL;
  }

  // Defer to the service to realloc
  log_fatal("realloc not implemented in alaska yet!");

  handle = alaska::Runtime::get().hrealloc(handle, new_size);
  return handle;
}



void hfree(void *ptr) {
  // no-op if NULL is passed
  if (ptr == NULL) return;

  // Grab the Mapping
  auto *m = alaska::Mapping::from_handle_safe(ptr);
  // Not a handle? Pass it to the system allocator.
  if (m == NULL) return ::free(ptr);

  // Simply ask the thread cache to free it!
  get_tc()->hfree(ptr);
}
