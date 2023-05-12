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

#include <alaska/internal.h>
#include <pthread.h>
#include <string.h>
#include <sys/mman.h>

// #define TABLE_START 0x80000000LU
#define TABLE_START 0x400000000LU

static pthread_mutex_t table_lock = PTHREAD_MUTEX_INITIALIZER;
// How many handles are in the table
static size_t table_size;
static size_t table_nfree;  // how many entries are free?
// A contiguous block of memory containing all the mappings
static alaska_mapping_t *table_memory;
static alaska_mapping_t *next_free = NULL;
static alaska_mapping_t *bump_next = NULL;


static void alaska_table_put_r(alaska_mapping_t *ent);

#define MAP_ENTRY_SIZE \
  sizeof(alaska_mapping_t)  // enforced by a static assert in translation_types.h
// #define MAP_GRANULARITY 0x200000LU               // minimum size of a "huge
// page" if we ever get around to that

#define MAP_GRANULARITY 0x1000LU * MAP_ENTRY_SIZE

// This file contains the implementation of the creation and management of the
// handle translation  It abstracts the management of the table's backing
// memory, as well as the get/put interface to allocate a new handle

alaska_mapping_t *alaska_table_begin(void) {
  return table_memory;
}
alaska_mapping_t *alaska_table_end(void) {
	return bump_next;
  // return table_memory + table_size;
}


// grow the table by a factor of 2
static void alaska_table_grow() {
  size_t oldbytes = table_size * MAP_ENTRY_SIZE;
  size_t newbytes = oldbytes * 2;
  // uint64_t start = alaska_timestamp();
  // printf("alaska: growing table\n");

  if (table_memory == NULL) {
    newbytes = MAP_GRANULARITY;
    // TODO: use hugetlbfs or something similar
    table_memory = (alaska_mapping_t *)mmap((void *)TABLE_START, newbytes, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
  } else {
    table_memory = (alaska_mapping_t *)mremap(table_memory, oldbytes, newbytes, 0, table_memory);
  }
  if (table_memory == MAP_FAILED) {
    fprintf(stderr, "could not resize table!\n");
    abort();
  }

  // newsize is now the number of entries
  size_t newsize = newbytes / MAP_ENTRY_SIZE;

  // the bump allocator
  bump_next = &table_memory[table_size];

  table_nfree += (newsize - table_size);

  // update the total size
  table_size = newsize;
}

void alaska_table_init(void) {
  pthread_mutex_init(&table_lock, NULL);
  pthread_mutex_lock(&table_lock);
  next_free = NULL;
  alaska_table_grow();
  pthread_mutex_unlock(&table_lock);
}

void alaska_table_deinit(void) {
  munmap(table_memory, table_size * MAP_ENTRY_SIZE);
}

static void alaska_table_put_r(alaska_mapping_t *ent) {
  // ent->size = -1;
  ent->ptr = next_free;
  table_nfree++;
  next_free = ent;
}

// allocate a table entry
alaska_mapping_t *alaska_table_get(void) {
  // pthread_mutex_lock(&table_lock);
  alaska_mapping_t *ent = NULL;

  if (table_nfree == 0) alaska_table_grow();

  if (next_free == NULL) {
    ent = bump_next;
    bump_next++;
  } else {
    ent = next_free;
    next_free = ent->ptr;
    __builtin_prefetch(next_free, 0, 0);
  }
  table_nfree--;

  // pthread_mutex_unlock(&table_lock);
  return ent;
}

// free a table entry
void alaska_table_put(alaska_mapping_t *ent) {
  // pthread_mutex_lock(&table_lock);
  alaska_table_put_r(ent);
  // pthread_mutex_unlock(&table_lock);
}
