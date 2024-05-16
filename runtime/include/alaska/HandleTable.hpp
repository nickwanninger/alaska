
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

  using slabidx_t = uint64_t;

  class HandleTable;

  class HandleSlab final {
   public:
    HandleSlab(HandleTable &table, slabidx_t idx);

    alaska::Mapping *get(void);
    void put(alaska::Mapping *m);


    inline auto nfree(void) const { return m_nfree; }

   private:
    HandleTable &m_table;  // Which table does this belong to?
    slabidx_t m_idx;       // Which slab is this?

    uint16_t m_nfree = 0;
    alaska::Mapping *m_next_free = nullptr;
    alaska::Mapping *m_bump_next = nullptr;
  };

  // This is a class which manages the mapping from pages in the handle table to slabs. If a
  // handle table is already allocated, this class will panic when being constructed.
  // In the actual runtime implementation, there will be a global instance of this class.
  class HandleTable final {
   public:
    HandleTable(void);
    ~HandleTable(void);



    static constexpr size_t slab_size = 0x1000;
    static constexpr size_t slab_capacity = slab_size / sizeof(alaska::Mapping);
    static constexpr size_t initial_capacity = 16;

    auto slab_count() const { return m_slabs.size(); }
    auto capacity() const { return m_capacity; }


    // Allocate a fresh slab, resizing the table if necessary.
    alaska::HandleSlab *fresh_slab(void);


    off_t mapping_slab_idx(Mapping *m);

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


    // How many slabs this table can hold (how big is the mmap region)
    uint64_t m_capacity;
    // The actual memory for the mmap region.
    alaska::Mapping *m_table;


    // How much the table increases it's capacity by each time it grows.
    static constexpr int growth_factor = 2;

    ck::vec<alaska::HandleSlab *> m_slabs;
  };
}  // namespace alaska