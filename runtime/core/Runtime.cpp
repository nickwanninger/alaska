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
#include <alaska/SizeClass.hpp>
#include "alaska/alaska.hpp"
#include <stdlib.h>

namespace alaska {
  static Runtime *g_runtime = nullptr;


  Runtime::Runtime() {
    ALASKA_ASSERT(g_runtime == nullptr, "Cannot create more than one runtime");
    g_runtime = this;

    log_debug("Created a new Alaska Runtime @ %p", this);
  }

  Runtime::~Runtime() {
    log_debug("Destroying Alaska Runtime");
    g_runtime = nullptr;
  }


  Runtime &Runtime::get() {
    ALASKA_ASSERT(g_runtime != nullptr, "Runtime not initialized");
    return *g_runtime;
  }


  // void* Runtime::halloc(size_t sz, bool zero) {
  //   auto& rt = *this;
  //   log_trace("halloc: allocating %lu bytes (zero=%d)", sz, zero);
  //   // Just some big number.
  //   if (sz > (1LLU << (uint64_t)(ALASKA_SIZE_BITS - ALASKA_SQUEEZE_BITS - 1)) - 1) {
  //     log_debug(
  //         "Allocation of size %zu deemed too big - using the system allocator instead...", sz);
  //     return ::malloc(sz);
  //   }



  //   int sc = alaska::size_to_class(sz);
  //   size_t rsz = alaska::class_to_size(sc);
  //   log_trace("sz = %zu, sc = %d, rsz = %zu\n", sz, sc, rsz);

  //   alaska::Mapping* m = rt.handle_table.get();
  //   if (m == nullptr) {
  //     log_fatal("halloc: out of handles");
  //     abort();
  //   }

  //   void* p = rt.page.alloc(*m, sz);

  //   if (p == nullptr) {
  //     log_fatal("halloc: out of memory");
  //     abort();
  //   }
  //   if (zero) {
  //     memset(p, 0, sz);
  //   }

  //   m->set_pointer(p);

  //   void* out = (void*)m->to_handle(0);
  //   log_trace("halloc handle %p -> %p", out, p);
  //   return out;
  // }

  // void Runtime::hfree(void* ptr) {
  //   auto* m = alaska::Mapping::from_handle_safe(ptr);

  //   log_trace("hfree: freeing %p (%p)", ptr, m->get_pointer());
  //   this->page.release(*m, m->get_pointer());
  // }



  // void* Runtime::hrealloc(void* ptr, size_t size) {
  //   log_trace("hrealloc(%p, %zu)", ptr, size);
  //   if (size == 0) {
  //     hfree(ptr);
  //     return nullptr;
  //   }

  //   // TODO: Implement hrealloc function
  //   log_fatal("hrealloc: not implemented");
  //   abort();
  //   return nullptr;
  // }



  void Runtime::dump(FILE *stream) {
    //
    fprintf(stream, "Alaska Runtime Information:\n");
    heap.dump(stream);
    handle_table.dump(stream);
  }



  ThreadCache *Runtime::new_threadcache(void) {
    auto tc = new ThreadCache(next_thread_cache_id++, *this);
    tcs_lock.lock();
    tcs.add(tc);
    tcs_lock.unlock();
    return tc;
  }

  void Runtime::del_threadcache(ThreadCache *tc) {
    tcs_lock.lock();
    tcs.remove(tc);
    delete tc;
    tcs_lock.unlock();
  }


}  // namespace alaska
