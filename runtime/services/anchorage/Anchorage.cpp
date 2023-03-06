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


#include <anchorage/Anchorage.hpp>
#include <anchorage/Chunk.hpp>
#include <anchorage/Block.hpp>
#include <alaska.h>
#include <alaska/internal.h>
#include <alaska/service/anchorage.h>
#include <alaska/vec.h>
#include <string.h>
#include <stdbool.h>
#include <sys/mman.h>

uint64_t next_last_access_time = 0;

#define DEBUG(...)

extern "C" void alaska_service_barrier(void) {
  // defer to anchorage
  anchorage::barrier();
}

extern "C" void alaska_service_alloc(alaska::Mapping *ent, size_t new_size) {
  // defer to anchorage
  anchorage::alloc(*ent, new_size);
}


extern "C" void alaska_service_free(alaska::Mapping *ent) {
  // defer to anchorage
  anchorage::free(*ent, ent->ptr);
}


extern "C" void alaska_service_init(void) {
  // defer to anchorage
  anchorage::allocator_init();
}

extern "C" void alaska_service_deinit(void) {
  // defer to anchorage
  anchorage::allocator_deinit();
}



void *anchorage::alloc(alaska::Mapping &m, size_t size) {
  // anchorage::barrier();

  Block *new_block = NULL;
  Chunk *new_chunk = NULL;
  // attempt to allocate from each chunk
  for (auto *chunk : anchorage::Chunk::all()) {
    auto blk = chunk->alloc(size);
    // if the chunk could allocate, use the pointer it created
    if (blk) {
      new_chunk = chunk;
      new_block = blk;
      break;
    }
  }
  (void)new_chunk;

  if (new_block == NULL) {
    printf("could not allocate. creating a new block w/ at least enough size for %zu\n", size);

    size_t required_pages = (size / anchorage::page_size) * 2;
    if (required_pages < anchorage::min_chunk_pages) {
      required_pages = anchorage::min_chunk_pages;
    }

    anchorage::barrier();                  // run a barrier
    new anchorage::Chunk(required_pages);  // add a chunk

    return anchorage::alloc(m, size);
  }

  if (m.ptr != NULL) {
    Block *old_block = anchorage::Block::get(m.ptr);
    old_block->clear_handle();
    size_t copy_size = m.size;
    if (size < copy_size) copy_size = size;
    memcpy(new_block->data(), m.ptr, copy_size);
    memset(m.ptr, 0xFA, m.size);
  }

  new_block->set_handle(&m);
  m.ptr = new_block->data();
  m.size = size;
  // printf("alloc: %p %p (%zu)", &m, m.ptr, m.size);
  // new_chunk->dump(new_block, "Alloc");
  return m.ptr;
}


void anchorage::free(alaska::Mapping &m, void *ptr) {
  auto *chunk = anchorage::Chunk::get(ptr);
  if (chunk == NULL) {
    fprintf(stderr, "[anchorage] attempt to free a pointer not managed by anchorage (%p)\n", ptr);

    return;
  }

  auto *blk = anchorage::Block::get(ptr);

  memset(m.ptr, 0xFA, m.size);
  if (m.anchorage.locks > 0) {
    // set the flag to indicate that it's free (but someone has a lock)
    m.anchorage.flags |= ANCHORAGE_FLAG_LAZY_FREE;
    printf("freed while locked. TODO!\n");

    return;
  }


  blk->clear_handle();
  m.size = 0;
  m.ptr = nullptr;
  // tell the block to coalesce the best it can
  blk->coalesce_free(*chunk);
  // anchorage::barrier();
}



size_t anchorage::moved_bytes = 0;
void *anchorage::memmove(void *dst, void *src, size_t size) {
  anchorage::moved_bytes += size;
  return ::memmove(dst, src, size);
}
