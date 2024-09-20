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
#include <alaska/alaska.hpp>
#include "ck/lock.h"

namespace alaska {

  struct Runtime;

  // A ThreadCache is the class which manages the thread-private
  // allocations out of a shared heap. The core runtime itself does
  // *not* manage where thread caches are used. It is assumed
  // something else manages storing a pointer to a ThreadCache in some
  // thread-local variable
  class ThreadCache final : public alaska::InternalHeapAllocated {
   public:
    ThreadCache(int id, alaska::Runtime &rt);

    void *halloc(size_t size, bool zero = false);
    void *hrealloc(void *handle, size_t new_size);
    void hfree(void *handle);

    int get_id(void) const { return this->id; }
    size_t get_size(void *handle);


    bool localize(alaska::Mapping &m, uint64_t epoch);
    bool localize(void *handle, uint64_t epoch);


   protected:
    friend class LockedThreadCache;
    friend alaska::Runtime;

    ck::mutex lock;

   private:
    // Allocate backing data for a handle, but don't assign it yet.
    void *allocate_backing_data(const alaska::Mapping &m, size_t size);

    // Free an allocation behind a handle, but not the handle
    void free_allocation(const alaska::Mapping &m);

    // Allocate a new handle table mapping
    alaska::Mapping *new_mapping(void);
    // Swap to a new sized page owned by this thread cache
    alaska::SizedPage *new_sized_page(int cls);
    // Swap to a new locality page owned by this thread cache
    alaska::LocalityPage *new_locality_page(size_t required_size);

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
    // it might allocate from. When a size class fills up, it is
    // returned to the global heap and another one is allocated.
    alaska::SizedPage *size_classes[alaska::num_size_classes] = {nullptr};
    // Each thread cache also has a private "Locality Page", which
    // objects can be relocated to according to some external
    // policy. This page is special because it can contain many
    // objects of many different sizes.
    alaska::LocalityPage *locality_page = nullptr;
  };



  class LockedThreadCache final {
   public:
    LockedThreadCache(ThreadCache &tc)
        : tc(tc) {
      tc.lock.lock();
    }


    ~LockedThreadCache(void) {
      tc.lock.unlock();
    }

    // Delete copy constructor and copy assignment operator
    LockedThreadCache(const LockedThreadCache &) = delete;
    LockedThreadCache &operator=(const LockedThreadCache &) = delete;

    // Delete move constructor and move assignment operator
    LockedThreadCache(LockedThreadCache &&) = delete;
    LockedThreadCache &operator=(LockedThreadCache &&) = delete;




    ThreadCache &operator*(void) { return tc; }
    ThreadCache *operator->(void) { return &tc; }

   private:
    ThreadCache &tc;
  };



}  // namespace alaska
