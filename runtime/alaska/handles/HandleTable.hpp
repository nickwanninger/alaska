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

#include <stdint.h>
#include <alaska/alaska.hpp>
#include <alaska/util/OwnedBy.hpp>
#include <alaska/heaps/HeapPage.hpp>
#include <ck/vec.h>
#include <ck/lock.h>
#include <alaska/util/SizedAllocator.hpp>
#include <alaska/Configuration.hpp>
#include <alaska/work/WorkScheduler.hpp>

namespace alaska {



  using slabidx_t = size_t;

  class HandleTable;
  class HandleSlabQueue;
  class ThreadCache;


  enum HandleSlabState {
    SlabStateEmpty,
    SlabStatePartial,
    SlabStateFull,
  };



  // A handle slab is a slice of the handle table that is used to allocate
  // mappings. It is a fixed size, and no two threads will allocate from the
  // same slab at the same time.
  struct HandleSlab final : public alaska::OwnedBy<alaska::ThreadCache>,
                            public alaska::PersistentAllocation {
   private:
    alaska::ShardedFreeList<DefaultFreeListBlock> free_list;  // A free list for tracking releases
    alaska::Mapping *start;
    alaska::Mapping *end;
    alaska::Mapping *next_free;  // Bump allocator.

   public:
    slabidx_t idx;                           // Which slab is this?
    HandleSlabState state = SlabStateEmpty;  // What is the state of this slab?
    HandleTable &table;                      // Which table does this belong to?

    HandleSlab *next = nullptr;                // The next slab in the queue
    HandleSlab *prev = nullptr;                // The previous slab in the queue
    HandleSlabQueue *current_queue = nullptr;  // What queue is this slab in?




    // -- Methods --
    HandleSlab(HandleTable &table, slabidx_t idx);
    virtual ~HandleSlab(void);


    // Implemented at the bottom of this file...
    alaska::Mapping *alloc(void);  // Allocate a mapping from this slab

    // Thread-safe free operation that can be called from any thread.
    // Uses atomic operations to safely return a mapping to this slab.
    __attribute__((always_inline)) inline void free(Mapping *m) {
      // TODO: local/free???
      free_list.free_local((void *)m);
    }

    void mlock(void);  // `mlock` the memory behind this slab


    inline alaska::Mapping *get_end(void) const { return end; }


    template <typename Fn>
    inline void for_each(Fn fn) {
      // TODO: optimize this by using the free list as a sort of skip
      // list to avoid visiting free mappings. That is, iterate over
      // the free list and check if the subsequent mapping is free or
      // not. If it is, iterate until it is not and apply the
      // callback. There is an edge case with the first entry in the
      // table, but that is easy enough to special case.
      for (alaska::Mapping *m = start; m < end; m++) {
        if (not m->is_free()) fn(m);
      }
    }


    bool contains(alaska::Mapping *m) const {
      // Check if the mapping is within the bounds of this slab
      return (m >= start && m < end);
    }

    bool allocated(alaska::Mapping *m) const {
      void *ptr = m->get_pointer();
      if (contains((alaska::Mapping *)ptr)) {
        return false;
      }
      return !m->is_free();
    }



    // Implmented in HandleTable.cpp, this function cannot be inlined and is meant
    // to be used when the local free list is empty.
    alaska::Mapping *alloc_slow(void);

    // Batch-extend the local free list from the bump allocator.
    // Returns the number of entries added.
    long extend(long count);

    // Reset this slab to a clean state for reuse.
    // Called by HandleTable when returning a slab to the free list.
    void reset(void);

    size_t num_free(void) const { return (end - next_free); }
    size_t capacity(void) const { return end - start; }
    bool has_any_free(void) const { return free_list.has_any_free() || (next_free < end); }

    inline auto &get_freelist(void) { return free_list; }
  };




  // A handle slab list is a doubly linked list of handle slabs.
  // It is used to keep track of groupings of slabs.
  class HandleSlabQueue final {
   public:
    void push(HandleSlab *slab);
    HandleSlab *pop(void);
    inline bool empty(void) const { return head == nullptr; }

    // remove a slab from the queue without popping it
    void remove(HandleSlab *slab);

   private:
    HandleSlab *head = nullptr;
    HandleSlab *tail = nullptr;
  };

  // HandleTable manages the global pool of handle slabs.
  //
  // Responsibilities:
  // - Allocate new handle slabs (from free list or bump allocator)
  // - Maintain global slab registry for index-based lookup and iteration
  // - Accept returned slabs and maintain a free list for recycling
  // - Support mapping->slab lookups via pointer arithmetic
  //
  // If a handle table is already allocated, construction will panic.
  // In the actual runtime implementation, there will be a global instance of this class.
  class HandleTable final {
   public:
    static constexpr size_t slab_size = 1 << 21;  // 21
    static constexpr size_t slab_capacity = slab_size / sizeof(alaska::Mapping);
    static constexpr size_t initial_capacity = 64;
    static constexpr size_t handle_count = (1UL << (63 - ALASKA_SIZE_BITS)) - 1;

    static constexpr size_t map_granularity = 1UL << 21;  // 2MB is the minimum mapping size.

    static constexpr size_t max_slab_count = handle_count / slab_capacity;

    HandleTable(const alaska::Configuration &config);
    ~HandleTable(void);

    // Allocate a fresh slab, resizing the table if necessary.
    // Tries free list first before bump allocating new slab.
    alaska::HandleSlab *fresh_slab(void);

    // Return a slab to the free list for recycling.
    // Performs full reset of slab state before adding to free list.
    void return_slab(HandleSlab *slab);
    alaska::HandleSlab *get_slab(slabidx_t idx);
    // Given a mapping, return the index of the slab it belongs to.
    slabidx_t mapping_slab_idx(Mapping *m) const;

    auto slab_count() const { return m_slabs.size(); }
    auto capacity() const { return m_capacity; }

    bool valid_handle(alaska::Mapping *m) const;



    // Get the slab that a mapping belongs to. This is done by using
    // pointer arithmetic to find the start of the slab. It is illegal
    // to call this function with a mapping which is not actually
    // allocated in the handle table, this does not perform checks.
    inline alaska::HandleSlab *get_slab(alaska::Mapping *m) {
      alaska::HandleSlab *slab = *(alaska::HandleSlab **)((uintptr_t)m & ~(slab_size - 1));
      return slab;
    }

    // Free/release *some* mapping using thread-safe atomic operations.
    // The owner parameter is now ignored; the slab uses atomic free operations
    // that work safely from any thread.
    inline void put(alaska::Mapping *m,
                    alaska::ThreadCache *owner = (alaska::ThreadCache *)0x1000UL) {
      alaska::HandleSlab *slab = get_slab(m);
      slab->free(m);
    }

    void *get_base(void) const { return (void *)m_table; }


    void enable_mlock() { do_mlock = true; }

    const ck::vec<alaska::HandleSlab *> &get_slabs(void) const { return m_slabs; }

    static int get_ht_fd(void);  // only valid on riscv under yukon


    // For each handle.
    template <typename Fn>
    inline void for_each_handle(Fn fn) {
      for (auto *slab : m_slabs)
        slab->for_each(fn);
    }

   protected:
    friend HandleSlab;

    inline alaska::Mapping *get_slab_start(slabidx_t idx) {
      return (alaska::Mapping *)((uintptr_t)m_table + idx * slab_size);
    }

    inline alaska::Mapping *get_slab_end(slabidx_t idx) {
      return (alaska::Mapping *)((uintptr_t)m_table + (idx + 1) * slab_size);
    }


   private:
    void grow();
    bool do_mlock = false;

    // A lock for the handle table
    ck::mutex lock;
    // How many slabs this table can hold (how big is the mmap region)
    uint64_t m_capacity;
    // The actual memory for the mmap region.
    alaska::Mapping *m_table;


    // How much the table increases it's capacity by each time it grows.
    static constexpr int growth_factor = 2;

    ck::vec<alaska::HandleSlab *> m_slabs;
    // Queue of free slabs available for recycling
    HandleSlabQueue m_free_slabs;
  };



  inline HandleSlab *HandleTable::get_slab(slabidx_t idx) {
    // ck::scoped_lock lk(this->lock);

    log_trace("Getting slab %d", idx);
    if (idx >= (slabidx_t)m_slabs.size()) {
      log_trace("Invalid slab requeset!");
      return nullptr;
    }
    return m_slabs[idx];
  }

  inline slabidx_t HandleTable::mapping_slab_idx(Mapping *m) const {
    auto byte_distance = (uintptr_t)m - (uintptr_t)m_table;
    return byte_distance / HandleTable::slab_size;
  }




  inline alaska::Mapping *HandleSlab::alloc(void) {
    // 1. Attempt to allocate a mapping from the free list.
    auto *m = (alaska::Mapping *)free_list.pop();
    // 2. If that fails, drop out to the slow path
    if (m == nullptr) {
      return alloc_slow();
    }
    return m;
  }




  // The last valid mapping in the handle table currently. This is set by the HandleTable whenever
  // it grows (since we can only have one of them anyways)
  extern alaska::Mapping *last_mapping;


  bool check_mapping(handle_id_t hid, alaska::Mapping *&out_m, void *&out_data);
  bool check_mapping(void *ptr, alaska::Mapping *&out_m, void *&out_data);

}  // namespace alaska
