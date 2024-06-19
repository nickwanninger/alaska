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

#include <alaska/Heap.hpp>
#include <alaska/HeapPage.hpp>
#include <alaska/HandleTable.hpp>

namespace alaska {

  struct Runtime;

  // A ThreadCache is the class which manages the thread-private
  // allocations out of a shared heap. The core runtime itself does
  // *not* manage where thread caches are used. It is assumed
  // something else manages storing a pointer to a ThreadCache in some
  // thread-local variable
  class ThreadCache final {
   public:
    ThreadCache(alaska::Runtime &rt);

    void *halloc(size_t size, bool zero = false);
    void *hrealloc(void *handle, size_t new_size);
    void hfree(void *handle);

   private:
    // Allocate a new handle table mapping
    alaska::Mapping *new_mapping(void);


    // A reference to the global runtime. This is here mainly to gain
    // access to the HandleTable and the Heap.
    alaska::Runtime &runtime;

    // A pointer to the current slab of the handle table that this thread
    // cache is allocating from.
    alaska::HandleSlab *handle_slab;

    // Each thread cache has a private heap page for each size class
    // it might allocate from. When a size class fills up, it is returned
    // to the global heap and another one is allocated.
    alaska::SizedPage *size_classes[alaska::num_size_classes] = { nullptr };
  };



}  // namespace alaska
