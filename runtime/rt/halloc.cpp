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

// #define MALLOC_BYPASS

#ifdef MALLOC_BYPASS
#include <malloc.h>
#endif

#include <ck/stack.h>
#include <ck/queue.h>
#include <ck/vec.h>
#include <ck/set.h>
#include <alaska/core/Runtime.hpp>
#include <alaska/core/ThreadCache.hpp>
#include <alaska.h>
#include <rt/access_trace.hpp>
#include <errno.h>
#include <stdlib.h>




alaska::LockedThreadCache get_tc(void) { return *alaska::ThreadCache::current(); }



void *alaska_halloc(size_t sz) noexcept {
#ifdef MALLOC_BYPASS
  return ::malloc(sz);
#endif
  void *result = alaska::halloc(sz);
  alaska::access_trace::alloc(result, sz);
  return result;
}

void *alaska_hcalloc(size_t nmemb, size_t size) {
#ifdef MALLOC_BYPASS
  return ::calloc(nmemb, size);
#endif
  void *result = alaska::hcalloc(nmemb, size);
  alaska::access_trace::alloc(result, nmemb * size);
  return result;
}

// Reallocate a handle
void *alaska_hrealloc(void *handle, size_t new_size) {
#ifdef MALLOC_BYPASS
  return ::realloc(handle, new_size);
#endif
  void *result = alaska::hrealloc(handle, new_size);
  if (result == handle) {
    // The same handle with a new size.
    alaska::access_trace::free(handle);
    alaska::access_trace::alloc(result, new_size);
  } else if (result != nullptr || new_size == 0) {
    alaska::access_trace::alloc(result, new_size);
    alaska::access_trace::free(handle);
  }
  return result;
}



void alaska_hfree(void *ptr) {
#ifdef MALLOC_BYPASS
  return ::free(ptr);
#endif
  alaska::access_trace::free(ptr);
  return alaska::hfree(ptr);
}




size_t alaska_usable_size(void *ptr) {
#ifdef MALLOC_BYPASS
  return ::malloc_usable_size(ptr);
#endif
  return alaska::halloc_usable_size(ptr);
}

