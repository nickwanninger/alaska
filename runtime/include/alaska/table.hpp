/*
 * This file is part of the Alaska Handle-Based Memory Management System
 *
 * Copyright (c) 2023, Nick Wanninger <ncw@u.northwestern.edu>
 * Copyright (c) 2023, The Constellation Project
 * All rights reserved.
 *
 * This is free software.  You are permitted to use, redistribute,
 * and modify it as specified in the file "LICENSE".
 */

#pragma once


#include <alaska/alaska.hpp>

namespace alaska {
  namespace table {
    // Allocate a table entry, returning a pointer to the mapping.
    auto get(void) -> alaska::Mapping *;
    // Release a table entry, allowing it to be used again in the future.
    auto put(alaska::Mapping *) -> void;

    // Allow iterating over each table entry. Typical C++ iteration
    auto begin(void) -> alaska::Mapping *;
    auto end(void) -> alaska::Mapping *;

    // Initialize and free the handle table
    void init();
    void deinit();


    // How many handles are managed by an extent?
    constexpr int handles_per_extent = 0x1000;
    // Each handle gets a nibble in the extent structure.
    constexpr int bits_per_handle = 4;
    // How much to grow the table by each time.
    constexpr int map_granularity = handles_per_extent * sizeof(alaska::Mapping);

    /** The handle table is a contiguious block of virtual memory, that we
     * logically break into "chunks" so we can store auxiliary metadata off of
     * the main handle table. Each of these Extent structures live on the system
     * heap (malloc) and a global vector stores them.
     *
     * Indexing that is a simple right shift of the handle ID.
     */
    class Extent {
     public:
      Extent(alaska::Mapping &m)
          : m_start_mapping(&m) {
      }


      inline alaska::Mapping *start_mapping(void) {
        return m_start_mapping;
      }

      bool contains(alaska::Mapping *);

      inline int num_free(void) {
        return m_num_free;
      }
      inline int num_alloc(void) {
        return alaska::table::handles_per_extent - m_num_free;
      }

      // Called as a callback when the handle is allocated or freed
      void allocated(alaska::Mapping *);
      void freed(alaska::Mapping *);


     private:
      uint16_t m_num_free = alaska::table::handles_per_extent;
      alaska::Mapping *m_start_mapping = NULL;
      // uint8_t m_bits[(handles_per_extent * bits_per_handle) / 8];
    };


    auto get_extent(alaska::Mapping *ent) -> alaska::table::Extent *;

  }  // namespace table
}  // namespace alaska
