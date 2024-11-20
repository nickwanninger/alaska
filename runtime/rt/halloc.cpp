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
#include <alaska/Runtime.hpp>
#include <alaska/ThreadCache.hpp>
#include <alaska.h>
#include <errno.h>



// TODO: don't have this be global!
static __thread alaska::ThreadCache *g_tc = nullptr;



alaska::ThreadCache *get_tc_r(void) {
  if (unlikely(g_tc == nullptr)) {
    g_tc = alaska::Runtime::get().new_threadcache();
  }
  return g_tc;
}
alaska::LockedThreadCache get_tc(void) { return *get_tc_r(); }




static void *_halloc(size_t sz, int zero) {
  void *result = get_tc()->halloc(sz, zero);

  // This seems right...
  if (result == NULL) errno = ENOMEM;
  return result;
}


void *halloc(size_t sz) noexcept {
#ifdef MALLOC_BYPASS
  return ::malloc(sz);
#endif
  return _halloc(sz, 0);
}

void *hcalloc(size_t nmemb, size_t size) {
#ifdef MALLOC_BYPASS
  return ::calloc(nmemb, size);
#endif
  return _halloc(nmemb * size, 1);
}

// Reallocate a handle
void *hrealloc(void *handle, size_t new_size) {
#ifdef MALLOC_BYPASS
  return ::realloc(handle, new_size);
#endif
  // If the handle is null, then this call is equivalent to malloc(size)
  if (handle == NULL) return halloc(new_size);


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

  handle = get_tc()->hrealloc(handle, new_size);
  return handle;
}



void hfree(void *ptr) {
#ifdef MALLOC_BYPASS
  return ::free(ptr);
#endif
  // no-op if NULL is passed
  if (unlikely(ptr == NULL)) return;

#ifdef ALASKA_HTLB_SIM
  extern void alaska_htlb_sim_invalidate(uintptr_t handle);
  alaska_htlb_sim_invalidate((uintptr_t)ptr);
#endif

  // Simply ask the thread cache to free it!
  get_tc()->hfree(ptr);
}




size_t alaska_usable_size(void *ptr) {
#ifdef MALLOC_BYPASS
  return ::malloc_usable_size(ptr);
#endif
  return get_tc()->get_size(ptr);
}




extern "C" bool localize_structure(void *ptr) {
  auto *m = alaska::Mapping::from_handle_safe(ptr);
  if (m == nullptr) return false;
  auto &rt = alaska::Runtime::get();

  // Trigger a barrier so we can move stuff
  return rt.with_barrier([&]() {
    auto *tc = get_tc_r();
    long max_depth = 17;
    ck::queue<void *> todo(max_depth);

    auto schedule_pointer = [&](void *h, alaska::Mapping *m) {
      if (m == NULL or not rt.handle_table.valid_handle(m) or m->is_free()) return;
      if (not m->is_pinned()) {
        tc->localize(*m, rt.localization_epoch);
      }

      if (todo.size() >= max_depth) {
        return;
      }
      todo.push(h);
    };

    // Fire off a check of the first pointer to bootstrap the localization
    schedule_pointer(ptr, m);

    while (not todo.is_empty()) {
      auto *h = todo.popo().unwrap();
      auto *m = alaska::Mapping::from_handle_safe(h);
      if (m == nullptr) continue;

      auto size = tc->get_size(h);
      // printf("%p, %zu\n", h, size / 8);

      size_t elements = size / 8;

      void **cursor = (void **)m->get_pointer();
      for (off_t e = 0; e < elements; e++) {
        void *c = cursor[e];
        auto *m = alaska::Mapping::from_handle_safe(c);
        if (m) {
          schedule_pointer(c, m);
        }
      }
    }
  });
}
