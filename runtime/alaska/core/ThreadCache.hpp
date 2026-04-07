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

#include <alaska/heaps/Heap.hpp>
#include <alaska/heaps/ArenaHeap.hpp>
#include <alaska/heaps/HeapPage.hpp>
#include <alaska/handles/HandleTable.hpp>

#include <alaska/heaps/LocalityPage.hpp>
#include <alaska/alaska.hpp>
#include "ck/lock.h"
#include <alaska/util/RateCounter.hpp>


#define TC_ALIGNED(p) ((__typeof__(p))__builtin_assume_aligned((p), sizeof(uintptr_t)))

namespace alaska {

  struct Runtime;

  // A ThreadCache is the class which manages the thread-private
  // allocations out of a shared heap. The core runtime itself does
  // *not* manage where thread caches are used. It is assumed
  // something else manages storing a pointer to a ThreadCache in some
  // thread-local variable
  class ThreadCache final : public alaska::PersistentAllocation {
   protected:
    friend class LockedThreadCache;
    friend alaska::Runtime;
    friend alaska::HeapPage;


    // Just an id for this thread cache assigned by the runtime upon creation. It's mostly
    // meaningless, meant for debugging.
    int id;


    // How many calls to the generic allocator have we had since the last 'collection'?
    long generic_count = 0;
    long generic_collect_count = 0;

    // A reference to the global runtime. This is here mainly to gain
    // access to the HandleTable and the Heap.
    alaska::Runtime &runtime;

    // Each ThreadCache now manages its own handle slab directly.
    // When the current slab is exhausted, the ThreadCache requests a new one from the HandleTable.
    alaska::HandleSlab *current_slab = nullptr;

   public:
    // A lock which is used to control access to this heap page. Mostly used to control
    // race conditions around barriers, as the rest of the heap can only be accessed through
    // a locked thread cache as a mediator
    ck::mutex lock;


    // Track allocation and free rates
    alaska::RateCounter allocation_rate;
    alaska::RateCounter free_rate;


    // How often are we getting a new heap or handle table?
    alaska::RateCounter heap_churn;
    alaska::RateCounter handle_table_churn;

   private:
    alaska::ArenaBlock *active_block = nullptr;

   public:
    ThreadCache(int id, alaska::Runtime &rt);
    ~ThreadCache();


    // Allocating data from a threadcache is broken into two
    // steps. First, we allocate a handle from the handle table. Then,
    // we allocate the data. Both of these steps have a 'fast path'
    // and a generic 'slow path'.  The core of both of the fast paths
    // is that they attempt to allocate by popping off a linked list.
    // If the linked list is empty, (or there is some other failure,
    // such as missing a HeapPage) then we drop into the generic slow
    // path.

    // Allocate a new handle table mapping
    alaska::Mapping *new_mapping(void);
    alaska::Mapping *new_mapping_generic(void);
    void free_mapping(alaska::Mapping *);


    alaska::ObjectHeader *allocate_object(size_t size, alaska::Mapping &mapping);
    alaska::ObjectHeader *allocate_object_generic(size_t size, alaska::Mapping &mapping);

    // Handle allocation and deallocation routines.
    void *halloc(size_t size) alaska_attr_malloc;
    void *halloc_generic(size_t size, alaska::Mapping &m) alaska_attr_malloc;
    void *halloc_generic_empty_ht(size_t size) alaska_attr_malloc;


    void *hrealloc(void *handle, size_t new_size) alaska_attr_malloc;
    void hfree(void *handle);


    // Non-handle allocation and deallocation routines.
    //    These routines are for 'baseline' measurements with our allocator, and
    //    shows the overhead of using handles with the same underlying
    //    allocator.
    // You SHOULD NOT use this function *and* the handle allocation routine
    // in the same execution context, as it will likely cause bugs.
    void *malloc_generic(size_t size, alaska::Mapping &m) alaska_attr_malloc;
    void *malloc(size_t size, bool zero = false) alaska_attr_malloc;
    void *realloc(void *ptr, size_t new_size) alaska_attr_malloc;
    void free(void *ptr);


    int get_id(void) const { return this->id; }
    size_t get_size(void *handle);


    struct LocalizationResult {
      size_t count;  // how many mappings were localized
    };
    // The thread cache is responsible for localizing a set of mappings to improve object
    // locality. This function takes a list of ordered mappings and lays them out contiguously
    // in memory and the mappings are updated to point to their new locations.
    LocalizationResult localize(alaska::handle_id_t *hids, size_t count);
    long localize(alaska::Mapping *mapping, long allowed_depth = 0, long depth = 0);
    long localize_one(alaska::Mapping *mapping);


    static ThreadCache *current() noexcept;

    void dump_info(FILE *f);

   private:
    alaska::Mapping *reverse_lookup(void *heap_ptr);

    void maybe_collect(size_t size);

    // Rotate to a page with free space, acquiring from global heap if needed.
    alaska::SizedPage *rotate_sized_page(int cls);
    // Swap to a new locality page owned by this thread cache
    alaska::LocalityPage *new_locality_page(size_t required_size);
  };




  class LockedThreadCache final {
   public:
    LockedThreadCache(ThreadCache &tc)
        : tc(tc) {
      // tc.lock.lock();
    }


    ~LockedThreadCache(void) {
      // tc.lock.unlock();
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


  // Free-function allocation interface. Defined alongside ThreadCache methods in
  // ThreadCache.cpp so the compiler can inline the TC lookup and dispatch together.
  void *halloc(size_t size) noexcept;
  void *hcalloc(size_t nmemb, size_t size) noexcept;
  void *hrealloc(void *handle, size_t new_size) noexcept;
  void hfree(void *handle) noexcept;
  size_t halloc_usable_size(void *handle) noexcept;

  // Pointer-returning allocation interface (no handles).
  void *stub_malloc(size_t size) noexcept;
  void *stub_calloc(size_t nmemb, size_t size) noexcept;
  void *stub_realloc(void *ptr, size_t new_size) noexcept;
  void stub_free(void *ptr) noexcept;
  size_t stub_malloc_usable_size(void *ptr) noexcept;









  inline alaska::Mapping *ThreadCache::new_mapping(void) {
    // Slab is guarenteed to be non-null, because it is allocated in the constructor of
    // ThreadCache.
    auto *slab = this->current_slab;

    auto &htfl = slab->get_freelist();

    auto *m = TC_ALIGNED(htfl.peek());

    if (m != nullptr) {
      htfl.pop_unchecked(m);
      return (alaska::Mapping *)m;
    }
    return new_mapping_generic();
  }

}  // namespace alaska
