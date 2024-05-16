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
#include <sys/mman.h>



namespace alaska {
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
  }


  HandleTable::~HandleTable() {
    // Release the handle table back to the OS
    munmap(m_table, m_capacity * HandleTable::slab_size);

    for (auto &slab : m_slabs) {
      delete slab;
    }
  }



  void HandleTable::grow() {
    auto new_cap = m_capacity * HandleTable::growth_factor;
    // Scale the capacity of the handle table

    // Grow the mmap region
    m_table = (alaska::Mapping *)mremap(
        m_table, m_capacity * HandleTable::slab_size, new_cap * HandleTable::slab_size, 0, m_table);

    m_capacity = new_cap;
    // Validate that the table was reallocated
    ALASKA_ASSERT(m_table != MAP_FAILED, "failed to reallocate handle table during growth");
  }


  HandleSlab *HandleTable::fresh_slab() {
    int idx = m_slabs.size();

    if (idx >= this->capacity()) {
      grow();
      ALASKA_ASSERT(idx < this->capacity(), "failed to grow handle table");
    }

    auto *sl = new HandleSlab(*this, idx);

    // Add the slab to the list of slabs
    m_slabs.push(sl);

    return sl;
  }



}  // namespace alaska
