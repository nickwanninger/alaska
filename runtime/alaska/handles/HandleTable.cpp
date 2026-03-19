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
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#include "alaska/util/utils.h"
#endif

#include <execinfo.h>
#include <unistd.h>


#include <alaska/handles/HandleTable.hpp>
#include <alaska/core/ThreadCache.hpp>
#include <alaska/util/Logger.hpp>
#include <alaska/heaps/HeapPage.hpp>
#include <ck/lock.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/prctl.h>

#include <stdlib.h>


static int dev_alaska_fd = -1;

namespace alaska {

  alaska::Mapping *last_mapping;

  int HandleTable::get_ht_fd(void) { return dev_alaska_fd; }

  //////////////////////
  // Handle Table
  //////////////////////
  HandleTable::HandleTable(const alaska::Configuration &config) {
    alaska::printf("Initializing handle table this=%p\n", this);
    FTR_FUNCTION();
    // We allocate a handle table to a fixed location. If that allocation fails,
    // we know that another handle table has already been allocated. Since we
    // don't have exceptions in this runtime we will just abort.

    uintptr_t table_start = config.handle_table_location;
    m_capacity = HandleTable::initial_capacity;


#ifndef ALASKA_YUKON_NO_HARDWARE
    if (dev_alaska_fd == -1 && getenv("ALASKA_ANON_HANDLE_TABLE") == nullptr) {
      int fd = open("/dev/alaska", O_RDWR);
      if (fd > 0) dev_alaska_fd = fd;
    }
#endif

    if (dev_alaska_fd > 0) {
      m_table = (Mapping *)mmap((void *)table_start, m_capacity * HandleTable::map_granularity,
                                PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, dev_alaska_fd, 0);
      alaska::printf("Yukon: allocated handle table to %p with the kernel module!\n", m_table);
    } else {
      // Attempt to allocate the initial memory for the table.
      m_table =
          (Mapping *)mmap((void *)table_start, m_capacity * HandleTable::map_granularity,
                          PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);

      alaska::printf("Allocated handle table to %p with anon mmap\n", m_table);
    }


    // Validate that the table was allocated
    ALASKA_ASSERT(m_table != MAP_FAILED,
                  "failed to allocate handle table. Maybe one is already allocated?");


    log_debug("handle table successfully allocated to %p with initial capacity of %lu", m_table,
              m_capacity);
  }

  HandleTable::~HandleTable() {
    FTR_FUNCTION();
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
    FTR_FUNCTION();
    auto new_cap = m_capacity * HandleTable::growth_factor;
    // Scale the capacity of the handle table
    log_debug("Growing handle table. New capacity: %lu, old: %lu", new_cap, m_capacity);

    // Grow the mmap region
    m_table = (alaska::Mapping *)mremap(m_table, m_capacity * HandleTable::map_granularity,
                                        new_cap * HandleTable::map_granularity, 0, m_table);

    m_capacity = new_cap;

    if (m_table == MAP_FAILED) {
      perror("failed to grow handle table.\n");
    }
    // Validate that the table was reallocated
    ALASKA_ASSERT(m_table != MAP_FAILED, "failed to reallocate handle table during growth");
  }

  HandleSlab *HandleTable::fresh_slab(void) {
    FTR_FUNCTION();
    ck::scoped_lock lk(this->lock);

    // Try to get a slab from the free list first
    HandleSlab *sl = m_free_slabs.pop();
    if (sl != nullptr) {
      // Reuse slab from free list
      log_trace("Reusing slab %lu from free list", sl->idx);
      return sl;
    }

    // No free slabs available, allocate a new one
    slabidx_t idx = m_slabs.size();
    log_trace("Allocating a new slab at idx %d", idx);

    // m_capacity is measured in map_granularity units
    // compute slab_capacity
    size_t capacity_in_slabs = (m_capacity * HandleTable::map_granularity) / HandleTable::slab_size;
    if (idx >= capacity_in_slabs) {
      log_debug("New slab requires more capacity in the table");
      grow();
      ALASKA_ASSERT(idx < this->capacity(), "failed to grow handle table");
    }

    // Allocate a new slab using the system allocator.
    sl = new HandleSlab(*this, idx);
    // Note: Slabs are now Domain-owned. ThreadCache owner is set to nullptr.
    // The owner field is kept for freelist routing (local vs remote operations) but does not
    // represent ownership.
    // TODO: Remove HandleSlab's ThreadCache ownership concept entirely
    sl->set_owner(nullptr);

    last_mapping = sl->get_end() + 1;

    // Add the slab to the list of slabs and return it
    m_slabs.push(sl);
    return sl;
  }

  void HandleTable::return_slab(HandleSlab *slab) {
    FTR_FUNCTION();
    ck::scoped_lock lk(this->lock);

    log_trace("Returning slab %lu to free list", slab->idx);

    // Full reset of slab state for clean reuse

    // Reset internal slab state (bump allocator and free lists)
    slab->reset();

    // Add to free list for recycling
    m_free_slabs.push(slab);
  }



  bool HandleTable::valid_handle(Mapping *m) const {
    auto idx = mapping_slab_idx(m);
    if (idx > (slabidx_t)m_slabs.size()) {
      return false;
    }

    auto *slab = m_slabs[idx];
    return slab->allocated(m);
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
    void *memory = table.get_slab_start(idx);
    // assert that memory is aligned to page size
    if ((((uintptr_t)memory % HandleTable::slab_size) != 0)) {
      log_fatal(
          "Handle slab memory is not aligned to page size. "
          "This is a bug in the Alaska runtime. Please report this bug to the Alaska "
          "developers.");
      abort();
    }

    *(HandleSlab **)memory = this;  // Set the slab pointer at the start of the memory
    // Then set the start, end, and next_free pointers to the next location
    this->start = this->next_free = (alaska::Mapping *)memory + 1;

    // Finally, set the end pointer to the end of the slab
    this->end = (alaska::Mapping *)memory + HandleTable::slab_capacity;
  }

  HandleSlab::~HandleSlab(void) {}


  long HandleSlab::extend(long count) {
    alaska::Mapping *batch_start = next_free;
    alaska::Mapping *batch_end = batch_start + count;
    if (batch_end > end) batch_end = end;

    long extended_count = batch_end - batch_start;
    if (extended_count <= 0) return 0;

    next_free = batch_end;

    // Insert in reverse order so the lowest-address mapping is first to be popped.
    for (alaska::Mapping *m = batch_end - 1; m >= batch_start; m--) {
      free_list.free_local(m);
    }

    return extended_count;
  }

  __attribute__((noinline)) alaska::Mapping *HandleSlab::alloc_slow(void) {
    // 1. try swapping the remote_free list and the local_free list.
    this->free_list.swap();
    if (this->free_list.has_local_free()) {
      return (alaska::Mapping *)this->free_list.pop();
    }

    // 2. If that doesn't work, try extending the list with the bump allocator.
    FTR_FUNCTION();
    long extended_count = extend(4096 / sizeof(alaska::Mapping));
    if (extended_count > 0) {
      return (alaska::Mapping *)free_list.pop();
    }

    // This check might be redundant since extend should only return 0
    // if there are no more mappings to allocate, but we check it just
    // in case.
    if (not free_list.has_local_free()) return nullptr;

    return (alaska::Mapping *)free_list.pop();
  }


  void HandleSlab::reset(void) {
    // Reset the bump allocator pointer to the start
    next_free = start;

    // Clear the free lists for clean reuse
    // TODO: Need to release allocated handles and data when dropped
    free_list.reset();
  }


  void HandleSlab::mlock(void) {
    // auto start = table.get_slab_start(idx);
    // ::mlock((void *)start, sizeof(alaska::Mapping) * HandleTable::slab_capacity);
  }



  bool check_mapping(handle_id_t hid, alaska::Mapping *&out_m, void *&out_data) {
    void *handle = alaska::Mapping::handle_from_hid(hid);
    return alaska::check_mapping(handle, out_m, out_data);
  }

  bool check_mapping(void *ptr, alaska::Mapping *&out_m, void *&out_data) {
    alaska::Mapping *m;
    void *data;

    m = alaska::Mapping::from_handle_safe(ptr);
    if (m == nullptr || m > last_mapping) return false;
    // if m is not aligned to 8 (any of the low 3 bits are set to 1), skip it.
    if (not IS_WORD_ALIGNED(m)) return false;
    // If it is free, skip it.
    if (m->is_free()) return false;

    data = m->get_pointer();
    // okay, check the page is valid.
    if (!alaska::Heap::get_page(data)) {
      return false;
    }

    out_m = m;
    out_data = data;
    return true;
  }
}  // namespace alaska
