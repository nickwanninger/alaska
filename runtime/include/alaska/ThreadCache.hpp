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
#include <alaska/LocalityPage.hpp>

namespace alaska {

  struct Runtime;

  // A ThreadCache is the class which manages the thread-private
  // allocations out of a shared heap. The core runtime itself does
  // *not* manage where thread caches are used. It is assumed
  // something else manages storing a pointer to a ThreadCache in some
  // thread-local variable
  class ThreadCache final {
   public:
    ThreadCache(int id, alaska::Runtime &rt);

    void *halloc(size_t size, bool zero = false);
    void *hrealloc(void *handle, size_t new_size);
    void hfree(void *handle);

    int get_id(void) const { return this->id; }

  private:

    // Allocate backing data for a handle, but don't assign it yet.
    void *allocate_backing_data(const alaska::Mapping &m, size_t size);

    // Free an allocation behind a handle, but not the handle
    void free_allocation(const alaska::Mapping &m);

    // Allocate a new handle table mapping
    alaska::Mapping *new_mapping(void);
    // Swap to a new sized page owned by this thread cache
    alaska::SizedPage *new_sized_page(int cls);


    // Just an id for this thread cache assigned by the runtime upon creation. It's mostly
    // meaningless, meant for debugging.
    int id;
    // A reference to the global runtime. This is here mainly to gain
    // access to the HandleTable and the Heap.
    alaska::Runtime &runtime;

    // A pointer to the current slab of the handle table that this thread
    // cache is allocating from.
    alaska::HandleSlab *handle_slab;

    // Each thread cache has a private heap page for each size class
    // it might allocate from. When a size class fills up, it is returned
    // to the global heap and another one is allocated.
    alaska::SizedPage *size_classes[alaska::num_size_classes] = {nullptr};


    alaska::LocalityPage *locality_page;
  };



}  // namespace alaska
