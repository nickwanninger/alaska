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


#include <alaska/HandleTable.hpp>
#include <alaska/ThreadCache.hpp>
#include <alaska/Logger.hpp>
#include <alaska/HeapPage.hpp>
#include "ck/lock.h"
#include <stdio.h>
#include <sys/mman.h>



namespace alaska {


  //////////////////////
  // Handle Table
  //////////////////////
  HandleTable::HandleTable() {
    // We allocate a handle table to a fixed location. If that allocation fails,
    // we know that another handle table has already been allocated. Since we
    // don't have exceptions in this runtime we will just abort.

    constexpr uintptr_t table_start =
        (0x8000000000000000LLU >> (ALASKA_SIZE_BITS - ALASKA_SQUEEZE_BITS));

    m_capacity = HandleTable::initial_capacity;

    // Attempt to allocate the initial memory for the table.
    m_table = (Mapping *)mmap((void *)table_start, m_capacity * HandleTable::slab_size,
        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);

    // Validate that the table was allocated
    ALASKA_ASSERT(
        m_table != MAP_FAILED, "failed to allocate handle table. Maybe one is already allocated?");


    log_debug("handle table successfully allocated to %p with initial capacity of %lu", m_table,
        m_capacity);
  }

  HandleTable::~HandleTable() {
    // Release the handle table back to the OS
    int r = munmap(m_table, m_capacity * HandleTable::slab_size);
    if (r < 0) {
      log_error("failed to release handle table memory to the OS");
    } else {
      log_debug("handle table memory released to the OS");
    }


    for (auto &slab : m_slabs) {
      log_trace("deleting slab %p (idx: %lu)", slab, slab->idx);
      delete (slab);
    }
  }


  void HandleTable::grow() {
    auto new_cap = m_capacity * HandleTable::growth_factor;
    // Scale the capacity of the handle table
    log_debug("Growing handle table. New capacity: %lu, old: %lu", new_cap, m_capacity);

    // Grow the mmap region
    m_table = (alaska::Mapping *)mremap(
        m_table, m_capacity * HandleTable::slab_size, new_cap * HandleTable::slab_size, 0, m_table);

    m_capacity = new_cap;
    // Validate that the table was reallocated
    ALASKA_ASSERT(m_table != MAP_FAILED, "failed to reallocate handle table during growth");
  }

  HandleSlab *HandleTable::fresh_slab(ThreadCache *new_owner) {
    ck::scoped_lock lk(this->lock);

    slabidx_t idx = m_slabs.size();
    log_trace("Allocating a new slab at idx %d", idx);

    if (idx >= this->capacity()) {
      log_debug("New slab requires more capacity in the table");
      grow();
      ALASKA_ASSERT(idx < this->capacity(), "failed to grow handle table");
    }

    // Allocate a new slab using the system allocator.
    // auto *sl = alaska::make_object<HandleSlab>(*this, idx);
    auto *sl = new HandleSlab(*this, idx);
    sl->set_owner(new_owner);

    // Add the slab to the list of slabs and return it
    m_slabs.push(sl);
    return sl;
  }


  HandleSlab *HandleTable::new_slab(ThreadCache *new_owner) {
    {
      ck::scoped_lock lk(this->lock);

      // TODO: PERFORMANCE BAD HERE. POP FROM A LIST!
      for (auto *slab : m_slabs) {
        log_trace("Attempting to allocate from slab %p (idx %lu)", slab, slab->idx);
        if (slab->get_owner() == nullptr && slab->allocator.num_free() > 0) {
          slab->set_owner(new_owner);
          return slab;
        }
      }
    }

    return fresh_slab(new_owner);
  }


  HandleSlab *HandleTable::get_slab(slabidx_t idx) {
    ck::scoped_lock lk(this->lock);

    log_trace("Getting slab %d", idx);
    if (idx >= m_slabs.size()) {
      log_trace("Invalid slab requeset!");
      return nullptr;
    }
    return m_slabs[idx];
  }

  slabidx_t HandleTable::mapping_slab_idx(Mapping *m) {
    auto byte_distance = (uintptr_t)m - (uintptr_t)m_table;
    return byte_distance / HandleTable::slab_size;
  }



  void HandleTable::dump(FILE *stream) {
    ck::scoped_lock lk(this->lock);

    // Dump the handle table in a nice debug output
    fprintf(stream, "Handle Table:\n");
    fprintf(stream, " - Size: %zu bytes\n", m_capacity * HandleTable::slab_size);
    for (auto *slab : m_slabs) {
      slab->dump(stream);
    }
  }


  void HandleTable::put(Mapping *m, alaska::ThreadCache *owner) {
    log_trace("Putting handle %p", m);
    // Validate that the handle is in this table
    ALASKA_ASSERT(
        mapping_slab_idx(m) < m_slabs.size(), "attempted to put a handle into the wrong table")

    // Get the slab that the handle is in
    auto *slab = m_slabs[mapping_slab_idx(m)];
    if (slab->is_owned_by(owner)) {
      slab->release_local(m);
    } else {
      slab->release_remote(m);
    }
  }



  //////////////////////
  // Handle Slab Queue
  //////////////////////
  void HandleSlabQueue::push(HandleSlab *slab) {
    slab->current_queue = this;

    // Initialize
    if (head == nullptr and tail == nullptr) {
      head = tail = slab;
    } else {
      tail->next = slab;
      slab->prev = tail;
      tail = slab;
    }
  }

  HandleSlab *HandleSlabQueue::pop(void) {
    if (head == nullptr) return nullptr;

    auto *slab = head;
    head = head->next;
    if (head != nullptr) {
      head->prev = nullptr;
    } else {
      tail = nullptr;
    }
    slab->prev = slab->next = nullptr;
    slab->current_queue = nullptr;
    return slab;
  }


  void HandleSlabQueue::remove(HandleSlab *slab) {
    if (slab->prev != nullptr) {
      slab->prev->next = slab->next;
    } else {
      head = slab->next;
    }

    if (slab->next != nullptr) {
      slab->next->prev = slab->prev;
    } else {
      tail = slab->prev;
    }

    slab->prev = slab->next = nullptr;

    slab->current_queue = nullptr;
  }




  //////////////////////
  // Handle Slab
  //////////////////////

  HandleSlab::HandleSlab(HandleTable &table, slabidx_t idx)
      : table(table)
      , idx(idx) {
    allocator.configure(
        table.get_slab_start(idx), sizeof(alaska::Mapping), HandleTable::slab_capacity);
  }



  Mapping *HandleSlab::alloc(void) {
    auto *m = (Mapping *)allocator.alloc();

    if (unlikely(m == nullptr)) return nullptr;
    update_state();
    return m;
  }

  void HandleSlab::release_remote(Mapping *m) {
    allocator.release_remote(m);
    update_state();
  }

  void HandleSlab::release_local(Mapping *m) {
    allocator.release_local(m);
    update_state();
  }

  void HandleSlab::update_state() {}


  void HandleSlab::dump(FILE *stream) {
    fprintf(stream, "Slab %4d | ", idx);
    fprintf(stream, "st %d | ", state);

    auto owner = this->get_owner();
    fprintf(stream, "owner: %4d | ", owner ? owner->get_id() : -1);
    fprintf(stream, "free %4d | ", allocator.num_free());

    fprintf(stream, "\n");
  }
}  // namespace alaska
