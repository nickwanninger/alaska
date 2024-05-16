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




namespace alaska {
  static Runtime* g_runtime = nullptr;


  Runtime::Runtime() {
    ALASKA_ASSERT(g_runtime == nullptr, "Cannot create more than one runtime");
    g_runtime = this;
  }

  Runtime::~Runtime() { g_runtime = nullptr; }


  Runtime& Runtime::get() {
    ALASKA_ASSERT(g_runtime != nullptr, "Runtime not initialized");
    return *g_runtime;
  }


  void* Runtime::halloc(size_t size) {
    // Simply call hrealloc with a null pointer. This is
    // how realloc in libc works.
    return this->hrealloc(NULL, size);
  }

  void Runtime::hfree(void* ptr) {
    // TODO: Implement hfree function
  }

  void* Runtime::hrealloc(void* ptr, size_t size) {
    if (size == 0) {
      hfree(ptr);
      return nullptr;
    }

    // TODO: Implement hrealloc function
    return nullptr;
  }

}  // namespace alaska