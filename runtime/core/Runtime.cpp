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
#include <stdlib.h>



namespace alaska {
  static Runtime* g_runtime = nullptr;


  Runtime::Runtime() {
    ALASKA_ASSERT(g_runtime == nullptr, "Cannot create more than one runtime");
    g_runtime = this;

    log_debug("Created a new Alaska Runtime @ %p", this);
  }

  Runtime::~Runtime() {
    log_debug("Destroying Alaska Runtime");
    g_runtime = nullptr;
  }


  Runtime& Runtime::get() {
    ALASKA_ASSERT(g_runtime != nullptr, "Runtime not initialized");
    return *g_runtime;
  }


  void* Runtime::halloc(size_t sz, bool zero) {
    auto& rt = *this;
    log_trace("halloc: allocating %lu bytes (zero=%d)", sz, zero);
    // Just some big number.
    if (sz > (1LLU << (uint64_t)(ALASKA_SIZE_BITS - ALASKA_SQUEEZE_BITS - 1)) - 1) {
      log_debug(
          "Allocation of size %zu deemed too big - using the system allocator instead...", sz);
      return ::malloc(sz);
    }

    alaska::Mapping* m = rt.handle_table.get();
    if (m == nullptr) {
      log_fatal("halloc: out of handles");
      abort();
    }

    void* p = rt.page.alloc(*m, sz);

    if (p == nullptr) {
      log_fatal("halloc: out of memory");
      abort();
    }
    if (zero) {
      memset(p, 0, sz);
    }

    m->set_pointer(p);

    void* out = (void*)m->to_handle(0);
    log_trace("halloc handle %p -> %p", out, p);
    return out;
  }

  void Runtime::hfree(void* ptr) {
    auto* m = alaska::Mapping::from_handle_safe(ptr);

    log_trace("hfree: freeing %p (%p)", ptr, m->get_pointer());
    this->page.release(*m, m->get_pointer());
  }



  void* Runtime::hrealloc(void* ptr, size_t size) {
    log_trace("hrealloc(%p, %zu)", ptr, size);
    if (size == 0) {
      hfree(ptr);
      return nullptr;
    }

    // TODO: Implement hrealloc function
    log_fatal("hrealloc: not implemented");
    abort();
    return nullptr;
  }



  void Runtime::dump(FILE* stream) {
    //
    fprintf(stream, "Alaska Runtime Information:\n");
    handle_table.dump(stream);
  }


}  // namespace alaska