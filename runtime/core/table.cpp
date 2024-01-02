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

// This file contains the implementation of the creation and management of the
// handle translation  It abstracts the management of the table's backing
// memory through a get/put interface to allocate and free handle mappings.

#define MAP_ENTRY_SIZE sizeof(alaska::Mapping)
#define MAP_GRANULARITY 0x1000LU * MAP_ENTRY_SIZE

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



// grow the table by a factor of 2
static void alaska_table_grow() {
  size_t oldbytes = table_size * MAP_ENTRY_SIZE;
  size_t newbytes = oldbytes * 2;

  if (table_memory == NULL) {
    newbytes = MAP_GRANULARITY;
    // TODO: use hugetlbfs or something similar
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
  size_t newsize = newbytes / MAP_ENTRY_SIZE;

  // Update the bump allocator to point to the new data.
  bump_next = &table_memory[table_size];
  table_nfree += (newsize - table_size);
  // update the total size
  table_size = newsize;
}


// allocate a table entry
alaska::Mapping *alaska::table::get(void) {
  pthread_mutex_lock(&table_lock);
  alaska::Mapping *ent = NULL;

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
