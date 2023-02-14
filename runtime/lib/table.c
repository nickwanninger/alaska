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

static pthread_mutex_t table_lock = PTHREAD_MUTEX_INITIALIZER;
// How many handles are in the table
static size_t table_size;
static size_t table_nfree;  // how many entries are free?
// A contiguous block of memory containing all the mappings
static alaska_mapping_t *table_memory;
static alaska_mapping_t *next_free;


static void alaska_table_put_r(alaska_mapping_t *ent);

#define MAP_ENTRY_SIZE sizeof(alaska_mapping_t)  // enforced by a static assert in translation_types.h
// #define MAP_GRANULARITY 0x200000LU               // minimum size of a "huge
// page" if we ever get around to that

#define MAP_GRANULARITY 0x1000LU * MAP_ENTRY_SIZE

// This file contains the implementation of the creation and management of the
// handle translation  It abstracts the management of the table's backing
// memory, as well as the get/put interface to allocate a new handle

alaska_mapping_t *alaska_table_begin(void) { return table_memory; }
alaska_mapping_t *alaska_table_end(void) { return table_memory + table_size; }

// static void dump_regions() {
//   FILE *f = fopen("/proc/self/maps", "r");
//   char line_buf[256];
//   while (!feof(f)) {
//     off_t start, end;
//     char flags[5];  // "rwxp\0"
//     if (fgets(line_buf, 256, f) == 0) break;
//
//     int count = sscanf(line_buf, "%lx-%lx %s\n", &start, &end, flags);
//     if (count == 3) {
//       printf("region:%s", line_buf);
//       if (flags[1] == 'w') {
//       }
//     }
//   }
//   fclose(f);
// }

// grow the table by a factor of 2
static void alaska_table_grow() {
  size_t oldbytes = table_size * MAP_ENTRY_SIZE;
  size_t newbytes = oldbytes * 2;

  if (table_memory == NULL) {
    newbytes = MAP_GRANULARITY;
    // TODO: use hugetlbfs or something similar
    table_memory = (alaska_mapping_t *)mmap(
        (void *)0x200000LU, newbytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
  } else {
    table_memory = (alaska_mapping_t *)mremap(table_memory, oldbytes, newbytes, 0, table_memory);
  }
  if (table_memory == MAP_FAILED) {
    fprintf(stderr, "could not resize table!\n");
    abort();
  }
  // newsize is now the number of entries
  size_t newsize = newbytes / MAP_ENTRY_SIZE;

  for (size_t i = newsize - 1; i > table_size; i--) {
    alaska_table_put_r(&table_memory[i]);
  }

  // update the total size
  table_size = newsize;
  // printf("alaska: grew table to %zu entries\n", size);
  // dump_regions();
}

void alaska_table_init(void) {
  pthread_mutex_init(&table_lock, NULL);
  pthread_mutex_lock(&table_lock);
  next_free = NULL;
  alaska_table_grow();
  pthread_mutex_unlock(&table_lock);
}

void alaska_table_deinit(void) { munmap(table_memory, table_size * MAP_ENTRY_SIZE); }

unsigned alaska_table_get_canonical(alaska_mapping_t *ent) { return (unsigned)(ent - table_memory); }
alaska_mapping_t *alaska_table_from_canonical(unsigned canon) { return &table_memory[canon]; }

static void alaska_table_put_r(alaska_mapping_t *ent) {
  ent->size = -1;
  ent->ptr = next_free;
  table_nfree++;
  next_free = ent;
}

// allocate a table entry
alaska_mapping_t *alaska_table_get(void) {
  pthread_mutex_lock(&table_lock);
  alaska_mapping_t *ent = NULL;

  if (next_free == NULL) {
    alaska_table_grow();
  }

  ent = next_free;
  table_nfree--;
  next_free = ent->ptr;

  memset(ent, 0, sizeof(*ent));

  pthread_mutex_unlock(&table_lock);
  return ent;
}

// free a table entry
void alaska_table_put(alaska_mapping_t *ent) {
  pthread_mutex_lock(&table_lock);
	alaska_table_put_r(ent);
  pthread_mutex_unlock(&table_lock);
}
