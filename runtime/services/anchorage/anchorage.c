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

#include <alaska.h>
#include <alaska/internal.h>
#include <alaska/service/anchorage.h>


static alaska_spinlock_t anch_lock = ALASKA_SPINLOCK_INIT;
#define BIG_LOCK() alaska_spin_lock(&anch_lock)
#define BIG_UNLOCK() alaska_spin_unlock(&anch_lock)




void alaska_service_init(void) {
  // Do nothing
}


void alaska_service_deinit(void) {
  // Do nothing
}


// The implementation for the anchorage allocator

// The Anchorage allocator is broken into two kinds of memory regions, chunks and blocks.
// A chunk is a macro allocation using mmap, and it contains many blocks. A block
// is the actual backing memory for each handle in the system. This chunk is *at most*
// 4mb in size (1024 pages). Any allocation larger than 2mb will be allocated by the
// system allocator instead.
typedef struct chunk {
  // TODO:
} chunk_t;

typedef struct block {
  // This is the metadata/header for each allocation.
  alaska_mapping_t *handle;
  struct block *next;
  uint8_t data[/* ... */];
} block_t;



void alaska_service_barrier(void) {
  BIG_LOCK();

  // Nothing
  BIG_UNLOCK();
}


void alaska_service_alloc(alaska_mapping_t *ent, size_t new_size) {
  BIG_LOCK();
  ent->ptr = realloc(ent->ptr, new_size);
  ent->size = new_size;
  BIG_UNLOCK();
}


void alaska_service_free(alaska_mapping_t *ent) {
  BIG_LOCK();
  free(ent->ptr);
  ent->ptr = NULL;
  BIG_UNLOCK();

  // TODO: try to defrag every once in a while
  alaska_service_barrier();
}
