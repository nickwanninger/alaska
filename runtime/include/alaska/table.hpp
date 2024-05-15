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

#include <stdint.h>
#include <alaska/alaska.hpp>

namespace alaska {
  namespace table {

    // A 'slab' is a thread-local private "cache" for allocating
    // handles within the handle table itself. Each Slab is a view of
    // a slice of the handle table, and allocations are only ever made
    // to a 'private' slab for that specific thread. When a handle is
    // returned to the slab, it follows closely the model of mimalloc,
    // where there are two free-lists. One free-list is designed for
    // thread local allocations, and no lock or atomics are used. The
    // second free-list is for concurrent frees (from a different
    // thread), and these lists are merged occasionally.
    //
    // Instances of the Slab class are allocated on the system heap
    // (using malloc)
    class Slab {
     public:
      // Allocate a handle from this slab. This may return NULL, which
      // indiciates that this slab is full. Also, this must only be
      // called from the owner thread.
      alaska::Mapping *allocate(void);

      // In order to free a handle in a slab, the caller must do the
      // heavy lifting to determine if the slab is local or remote and
      // call the right function themselves.
      void local_free(alaska::Mapping *);
      void remote_free(alaska::Mapping *);

      // If any local free entries are available, return true.
      bool any_local_free(void) { return local_freelist != NULL; }


      Slab(int idx);



     private:
      alaska::Mapping *start(void);

      // the index into the slab array (and consequently the offset
      // into the whole handle table)
      int slab_idx;
      uint16_t capacity;
      alaska::Mapping *local_freelist = NULL;
      alaska::Mapping *remote_freelist = NULL;
    };


    constexpr size_t slab_size = 0x1000;
    constexpr size_t initial_table_size_bytes = slab_size * 32;
    constexpr int scaling_factor = 2;


    // Allocation and management of slabs themselves.
    alaska::table::Slab *fresh_slab(void);  // Allocate a new slab, do not look in the slab queue.

    // The following functions surround the management of the 'local'
    // slab for a given thread. Internally, it is stored as a
    // `thread_local` variable.
    void set_local_slab(Slab *sl);
    Slab *get_local_slab(void);
    void abandon_slab(Slab *sl);


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

  }  // namespace table
}  // namespace alaska
