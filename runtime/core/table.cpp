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

// A lock on the table itself.
// TODO: how do we make this atomic? Can we even?
static pthread_mutex_t table_lock = PTHREAD_MUTEX_INITIALIZER;

// How many handles are in the table
static size_t table_size;
static size_t table_nfree;  // how many entries are free?


// A contiguous block of memory containing all the mappings
static alaska::Mapping *table_memory;
// When allocating from the free list, pull from here.
static alaska::Mapping *next_free = NULL;
// When bump allocating, grab from here and increment it.
static alaska::Mapping *bump_next = NULL;

static long num_extents = 0;
static alaska::table::Extent **extents = NULL;

// grow the table by a factor of 2
static void alaska_table_grow() {
  printf("Grow!\n");
  size_t oldbytes = table_size * sizeof(alaska::Mapping);
  size_t newbytes = oldbytes * 2;

  if (table_memory == NULL) {
    newbytes = alaska::table::map_granularity;
    table_memory = (alaska::Mapping *)mmap((void *)TABLE_START, newbytes, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
  } else {
    table_memory = (alaska::Mapping *)mremap(table_memory, oldbytes, newbytes, 0, table_memory);
  }


  if (table_memory == MAP_FAILED) {
    fprintf(stderr, "could not resize table!\n");
    abort();
  }
  // newsize is now the number of entries
  size_t newsize = newbytes / sizeof(alaska::Mapping);


  // Update the bump allocator to point to the new data.
  bump_next = &table_memory[table_size];
  table_nfree += (newsize - table_size);
  // update the total size
  table_size = newsize;
  // Update the extents.
  long new_num_extents = newsize / alaska::table::handles_per_extent;


  extents =
      (alaska::table::Extent **)realloc(extents, new_num_extents * sizeof(alaska::table::Extent *));
  for (long e = num_extents; e < new_num_extents; e++) {
    auto ext = new alaska::table::Extent(*(bump_next + (e * alaska::table::handles_per_extent)));
    printf("create extent %ld, %p\n", e, ext);
    extents[e] = ext;
  }

  num_extents = new_num_extents;
}



static void dump_extents(void) {
  for (long e = 0; e < num_extents; e++) {
    auto *ext = extents[e];
    printf(" - %p a:%4d f:%4d\n", ext, ext->num_alloc(), ext->num_free());
  }
}




// allocate a table entry
alaska::Mapping *alaska::table::get(void) {
  pthread_mutex_lock(&table_lock);
  alaska::Mapping *ent = NULL;

  printf("table_nfree = %zu\n", table_nfree);

  if (table_nfree == 0 || bump_next >= table_memory + table_size) {
    alaska_table_grow();
  }

  if (next_free == NULL) {
    // Bump allocate
    ent = bump_next;
    bump_next++;
  } else {
    // Pop from the free list
    ent = next_free;
    next_free = ent->get_next();
  }
  table_nfree--;

  // Clear the entry.
  ent->reset();
  auto ext = alaska::table::get_extent(ent);
  ext->allocated(ent);
  dump_extents();

  pthread_mutex_unlock(&table_lock);
  return ent;
}


void alaska::table::put(alaska::Mapping *ent) {
  pthread_mutex_lock(&table_lock);
  ent->reset();
  // Increment the number of free handles
  table_nfree++;
  // Update the free list
  ent->set_next(next_free);
  auto ext = alaska::table::get_extent(ent);
  ext->freed(ent);
  dump_extents();

  next_free = ent;
  pthread_mutex_unlock(&table_lock);
}


void alaska::table::init() {
  pthread_mutex_init(&table_lock, NULL);
  pthread_mutex_lock(&table_lock);
  next_free = NULL;
  alaska_table_grow();
  pthread_mutex_unlock(&table_lock);
}

void alaska::table::deinit() {
}

alaska::Mapping *alaska::table::begin(void) {
  return table_memory;
}
alaska::Mapping *alaska::table::end(void) {
  return bump_next;
}




bool alaska::table::Extent::contains(alaska::Mapping *ent) {
  return ent >= start_mapping() and ent < start_mapping() + alaska::table::handles_per_extent;
}


void alaska::table::Extent::allocated(alaska::Mapping *ent) {
  m_num_free--;
}

void alaska::table::Extent::freed(alaska::Mapping *ent) {
  m_num_free++;
}

alaska::table::Extent *alaska::table::get_extent(alaska::Mapping *ent) {
  ALASKA_ASSERT(ent != NULL, "Mapping must not be null");
  uint64_t index = (uint64_t)ent - (uint64_t)alaska::table::begin();

  printf("index = %zx\n", index);
  index /= sizeof(alaska::Mapping);
  printf("        %zx\n", index);
  index /= alaska::table::handles_per_extent;
  printf("        %zx\n", index);

  auto *ext = extents[index];
  ALASKA_ASSERT(ext->contains(ent), "Nonsense extent map");
  return ext;
}

