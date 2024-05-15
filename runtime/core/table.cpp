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

#include <alaska/alaska.hpp>
#include <alaska/table.hpp>
#include <pthread.h>
#include <string.h>
#include <sys/mman.h>
#include <ck/vec.h>

// This file contains the implementation of the creation and management of the
// handle translation  It abstracts the management of the table's backing
// memory through a get/put interface to allocate and free handle mappings.

static constexpr size_t map_entry_size = sizeof(alaska::Mapping);
static constexpr size_t map_granularity = map_entry_size * 0x1000LU;
static constexpr off_t table_start =
    (0x8000000000000000LLU >> (ALASKA_SIZE_BITS - ALASKA_SQUEEZE_BITS));




// A lock on the table itself.
// TODO: how do we make this atomic? Can we even?
static pthread_mutex_t table_lock = PTHREAD_MUTEX_INITIALIZER;

// How many handles are in the table
static size_t table_nfree;  // how many entries are free?

// A contiguous block of memory containing all the mappings. This is located at
// a fixed address in memory (table_start) and is managed by the rest of this file.
static alaska::Mapping *table_memory;
// The current capacity of this table, in the number of slabs.
static uint64_t table_capacity = 0;
ck::vec<alaska::table::Slab *> slabs;



// Return the number of slabs in the handle table so far.
static size_t table_size() { return slabs.size(); }
static size_t table_size_bytes() { return slabs.size() * alaska::table::slab_size; }


// grow the table by a factor of 2
static void alaska_table_grow() {
  // All we do here is expand the table's capacity by a factor of 2 in virtual memory. There is not
  // any physical memory allocation here. The `slab` vector is not updated here, as that is handled
  // when calling `fresh_slab`.
  size_t oldbytes = table_capacity * alaska::table::slab_size;
  // The new size is the old size times the scaling factor
  size_t newbytes = oldbytes * alaska::table::scaling_factor;


  if (table_memory == nullptr) {
    newbytes = alaska::table::initial_table_size_bytes;
    // If the table memory is null, we need to allocate a new table
    table_memory = (alaska::Mapping *)mmap((void *)table_start, newbytes, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
  } else {
    // If the table memory is not null, we need to resize the table
    table_memory = (alaska::Mapping *)mremap(table_memory, oldbytes, newbytes, 0, table_memory);
  }


  if (table_memory == MAP_FAILED) {
    fprintf(stderr, "Could not resize table!\n");
    abort();
  }

  printf("table_memory: %p %zu\n", table_memory, newbytes);

  table_capacity = newbytes / alaska::table::slab_size;
}



// allocate a table entry
alaska::Mapping *alaska::table::get(void) {
  auto slab = get_local_slab();
  printf("slab = %p\n", slab);
  return nullptr;
}


void alaska::table::put(alaska::Mapping *ent) {
  fprintf(stderr, "PUT not implemented\n");
  abort();
}




alaska::table::Slab *alaska::table::fresh_slab(void) {
  if (table_size() == table_capacity) {
    alaska_table_grow();
  }

  ALASKA_ASSERT(table_size() < table_capacity, "Failed to resize the table");

  // Now, simply allocate a new slab (on the system heap), and push it to the slab vector.
  auto slab = new alaska::table::Slab(table_size());
  printf("Fresh slab: %p\n", slab);
  slabs.push(slab);
  return slab;
}


void alaska::table::init() {
  // alaska_table_grow();
}
void alaska::table::deinit() {}




alaska::Mapping *alaska::table::begin(void) { return table_memory; }


alaska::Mapping *alaska::table::end(void) { return nullptr; }




static thread_local alaska::table::Slab *local_slab = nullptr;
void alaska::table::set_local_slab(alaska::table::Slab *sl) {
  ALASKA_ASSERT(local_slab == nullptr, "Cannot set local slab when there already is one");
  printf("setting local_slab from %p to %p\n", local_slab, sl);
  local_slab = sl;
}


alaska::table::Slab *alaska::table::get_local_slab(void) {
  // Grab a new slab if there isn't one already set.
  // And, If the local slab is full, get a new one.
  if (local_slab == nullptr or not local_slab->any_local_free()) {
    // Get a new slab
    auto slab = fresh_slab();
    // And update the local slab
    set_local_slab(slab);
  }
  return local_slab;
}




alaska::Mapping *alaska::table::Slab::allocate(void) { return nullptr; }




alaska::table::Slab::Slab(int idx)
    : slab_idx(idx) {
  capacity = 0;
  local_freelist = NULL;
  remote_freelist = NULL;

  // TODO: how do we want to do this?
}


alaska::Mapping *alaska::table::Slab::start(void) {
  return (alaska::Mapping *)((uintptr_t)table_memory + (slab_idx * alaska::table::slab_size));
}