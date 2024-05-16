
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
#include <ck/vec.h>


namespace alaska {

  class HandleTable;

  class HandleSlab final {
   public:
    HandleSlab(HandleTable &table, int idx)
        : m_table(table)
        , m_idx(idx) {}


   private:
    HandleTable &m_table;  // Which table does this belong to?
    int m_idx;             // Which slab is this?
  };

  // This is a class which manages the mapping from pages in the handle table to slabs. If a
  // handle table is already allocated, this class will panic when being constructed.
  // In the actual runtime implementation, there will be a global instance of this class.
  class HandleTable final {
   public:
    HandleTable(void);
    ~HandleTable(void);



    static constexpr size_t slab_size = 0x1000;
    static constexpr size_t initial_capacity = 16;

    auto slab_count() const { return m_slabs.size(); }
    auto capacity() const { return m_capacity; }


    // Allocate a fresh slab, resizing the table if necessary.
    alaska::HandleSlab *fresh_slab(void);

   private:
    void grow();


    // How many slabs this table can hold (how big is the mmap region)
    uint64_t m_capacity;
    // The actual memory for the mmap region.
    alaska::Mapping *m_table;


    // How much the table increases it's capacity by each time it grows.
    static constexpr int growth_factor = 2;

    ck::vec<alaska::HandleSlab *> m_slabs;
  };
}  // namespace alaska