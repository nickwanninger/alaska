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
#include <alaska/heaps/HeapPage.hpp>
#include <alaska/handles/HandleTable.hpp>

#include <alaska/heaps/LocalityPage.hpp>
#include <alaska/alaska.hpp>
#include <alaska/Localizer.hpp>
#include "ck/lock.h"
#include <alaska/util/RateCounter.hpp>

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
    friend alaska::Localizer;
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

    // Each thread cache has a localizer, which can be fed with
    // "localization data" to improve object locality
    alaska::Localizer localizer;

  private:
    // Each thread cache has a private heap page for each size class
    // it might allocate from. When a size class fills up, it is
    // returned to the global heap and another one is allocated.
    alaska::SizedPage *size_classes[alaska::num_size_classes];

    // Each thread cache also has a private "Locality Page", which
    // objects can be relocated to according to some external
    // policy. This page is special because it can contain many
    // objects of many different sizes.
    alaska::LocalityPage *locality_page = nullptr;

   public:
    ThreadCache(int id, alaska::Runtime &rt);

    // Handle allocation and deallocation routines.
    void *halloc(size_t size) alaska_attr_malloc;

    void *halloc_generic(size_t size) alaska_attr_malloc;


    void *hrealloc(void *handle, size_t new_size) alaska_attr_malloc;
    void hfree(void *handle);


    // Non-handle allocation and deallocation routines.
    //    These routines are for 'baseline' measurements with our allocator, and
    //    shows the overhead of using handles with the same underlying
    //    allocator.
    // You SHOULD NOT use this function *and* the handle allocation routine
    // in the same execution context, as it will likely cause bugs.
    void *malloc_generic(size_t size) alaska_attr_malloc;
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
    // This function is called from the Localizer class.
    LocalizationResult localize(alaska::handle_id_t *hids, size_t count);
    static constexpr uint64_t hotness_hist_size = 1 << 6;
    uint64_t localization_epoch = 0;
    uint64_t hotness_hist[hotness_hist_size] = {0};
    long localize(alaska::Mapping *mapping, long allowed_depth = 0, long depth = 0);
    long localize_one(alaska::Mapping *mapping);

    // Allocate a new handle table mapping
    alaska::Mapping *new_mapping(void);
    alaska::Mapping *new_mapping_slow_path(void);
    void free_mapping(alaska::Mapping *);

    static ThreadCache *current(void);

   private:
    alaska::Mapping *reverse_lookup(void *heap_ptr);

    void maybe_collect(size_t size);

    // Swap to a new sized page owned by this thread cache
    alaska::SizedPage *new_sized_page(int cls);
    // Swap to a new locality page owned by this thread cache
    alaska::LocalityPage *new_locality_page(size_t required_size);
  };


  inline alaska::Mapping *ThreadCache::new_mapping(void) {
    if (unlikely(current_slab == nullptr || !current_slab->has_any_free())) {
      // Slow path: find/allocate a new slab
      return new_mapping_slow_path();
    }
    auto m = current_slab->alloc();
    if (unlikely(m == nullptr)) {
      // Slab appeared to have space but allocation failed, try slow path
      return new_mapping_slow_path();
    }
    return m;
  }




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



}  // namespace alaska
