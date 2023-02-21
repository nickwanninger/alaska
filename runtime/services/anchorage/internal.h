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
#include <alaska.h>
#include <alaska/internal.h>
#include <alaska/service/anchorage.h>
#include <alaska/list_head.h>


// The minimum size for a chunk (in pages). This is a lower bound
// as we may need to allocate *large* allocations. If an allocation
// is made that is larger than a chunk, a new chunk will be created
// that is a multiple of `CHUNK_PAGE_GRANULARITY` such that the
// allocation can fit inside.
#define CHUNK_PAGE_GRANULARITY 1024
#define PGSIZE 4096

#define BLOCK_GRANULARITY 16


typedef struct block {
  // This is the metadata/header for each allocation.
  struct block *next;        // Intrusive list.
  alaska_mapping_t *handle;  // If this is NULL, this block is unallocated
} block_t;

#define SIZE_WITH_OVERHEAD(sz) ((sz) + sizeof(block_t))


typedef struct chunk {
  // Doubly linked list to all the chunks
  struct list_head node;
  size_t pages;  // How many 4k pages this chunk uses
  void *data;    // The memory mapped to this chunk
  block_t *blocks;
  block_t *tos;  // "Top of stack"
} chunk_t;


// Allocate a chunk with a certain number of pages backing it.
chunk_t *add_chunk(size_t pages);
void del_chunk(chunk_t *chunk);

// Attempt to bump allocate a new block on a chunk. If it fails
// (we can't fit the allocation in this chunk) return NULL. This
// indicates you should try another chunk, or attempt to defrag
// this chunk or defrag the whole system (cross chunks)
block_t *chunk_bump_alloc(chunk_t *chunk, size_t size);
