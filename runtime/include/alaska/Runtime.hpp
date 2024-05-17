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

#pragma once
#include <alaska/HandleTable.hpp>
#include <alaska/Logger.hpp>

namespace alaska {

  /**
   * @brief The Runtime class is a container for the global state of the Alaska runtime.
   *
   * It is encapsulated in this class to allow for easy testing of the runtime. The application
   * links against a separate library which provides an interface to a singleton instance of this
   * through a C API (see `alaska.h`, ex: halloc()).
   *
   * This class' main job is to tie together all the various components of the runtime. These
   * components should operate entirely independently of each other, and this class should be the
   * only place where they interact.
   *
   * Components:
   *  - HandleTable: A global table that maps handles to their corresponding memory blocks.
   *  - Heap: A global memory manager that allocates and frees memory blocks. (Anchorage)
   *  - BarrierManager: The system which manages the delivery of patch-based barriers
   */
  struct Runtime final {
    // The handle table is a global table that maps handles to their corresponding memory blocks.
    alaska::HandleTable handle_table;

    // TODO:
    // alaska::Heap allocator;
    // alaska::BarrierManager barrier;


    // Return the singleton instance of the Runtime if it has been allocated. Abort otherwise.
    static Runtime &get();


    Runtime();
    ~Runtime();


    // The main interfaces to the runtime. This function is called by the C API.
    void *halloc(size_t size, bool zero = false);
    void hfree(void *ptr);
    void *hrealloc(void *ptr, size_t size);

    void dump(FILE *stream);
  };
}  // namespace alaska