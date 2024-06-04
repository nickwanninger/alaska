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
#include <alaska/Logger.hpp>
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
      delete slab;
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

  HandleSlab *HandleTable::fresh_slab() {
    slabidx_t idx = m_slabs.size();
    log_trace("Allocating a new slab at idx %d", idx);

    if (idx >= this->capacity()) {
      log_debug("New slab requires more capacity in the table");
      grow();
      ALASKA_ASSERT(idx < this->capacity(), "failed to grow handle table");
    }

    // Allocate a new slab using the system allocator.
    auto *sl = new HandleSlab(*this, idx);

    // Add the slab to the list of slabs and return it
    m_slabs.push(sl);
    return sl;
  }


  HandleSlab *HandleTable::get_slab(slabidx_t idx) {
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
    // Dump the handle table in a nice debug output
    fprintf(stream, "Handle Table:\n");
    fprintf(stream, " - Size: %zu bytes\n", m_capacity * HandleTable::slab_size);
    for (auto *slab : m_slabs) {
      slab->dump(stream);
    }
  }



  Mapping *HandleTable::get(void) {
    // NAIVE: Just attempt to allocate from each slab

    for (auto *slab : m_slabs) {
      log_trace("Attempting to allocate from slab %p (idx %lu)", slab, slab->idx);
      auto *m = slab->get();
      if (m != nullptr) {
        log_trace("Success. Allocated mapping %p", m);
        return m;
      }
    }

    log_debug("No slab has any handles. Asking for a fresh one");
    // if we get here we need to allocate a new slab
    return fresh_slab()->get();
  }


  void HandleTable::put(Mapping *m) {
    log_trace("Putting handle %p", m);
    // Validate that the handle is in this table
    ALASKA_ASSERT(
        mapping_slab_idx(m) < m_slabs.size(), "attempted to put a handle into the wrong table")

    // Get the slab that the handle is in
    auto *slab = m_slabs[mapping_slab_idx(m)];
    slab->put(m);
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
    bump_next = table.get_slab_start(idx);
    nfree = table.get_slab_end(idx) - bump_next;
  }



  Mapping *HandleSlab::get(void) {
    // We always try to pop from the free-list to reuse the mappings which can reduce fragmentation
    // compared to always bump allocating.

    // Pop from the next_free list if it is not null.
    if (next_free != nullptr) {
      nfree--;
      auto *ret = next_free;
      next_free = next_free->get_next();
      update_state();
      return ret;
    }

    // If the next_free list is null, we need to bump the next pointer
    if (bump_next < table.get_slab_end(idx)) {
      nfree--;
      update_state();
      return bump_next++;
    }

    return nullptr;
  }


  void HandleSlab::put(Mapping *m) {
    // Validate that the handle is in this slab.
    ALASKA_ASSERT(idx == table.mapping_slab_idx(m), "attempted to put a handle into the wrong slab")

    // Increment the number of free mappings
    nfree++;
    m->set_next(next_free);
    next_free = m;
    update_state();
  }

  void HandleSlab::update_state() {
    switch (nfree) {
      case 0:
        state = SlabStateFull;
        break;
      case HandleTable::slab_capacity:
        state = SlabStateEmpty;
        break;
      default:
        state = SlabStatePartial;
        break;
    }
  }


  void HandleSlab::dump(FILE *stream) {
    fprintf(stream, "Slab %4d | ", idx);
    fprintf(stream, "st %d | ", state);

    fprintf(stream, "free %4d | ", nfree);
    fprintf(stream, "bump %016p | ", bump_next);
    fprintf(stream, "next %016p | ", next_free);



    fprintf(stream, "\n");
  }
}  // namespace alaska