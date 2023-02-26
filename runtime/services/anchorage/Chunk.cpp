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

#include "./allocator.h"

#include <alaska.h>
#include <alaska/internal.h>
#include <alaska/service/anchorage.h>
#include <alaska/vec.h>
#include <string.h>
#include <sys/mman.h>
#include <unordered_set>
#include <assert.h>



// The global set of chunks in the allocator
static std::unordered_set<anchorage::Chunk *> *all_chunks;


void *anchorage::alloc(alaska::Mapping &m, size_t size) {
  // anchorage::barrier();

  Block *new_block = NULL;
  Chunk *new_chunk = NULL;
  // attempt to allocate from each chunk
  for (auto *chunk : *all_chunks) {
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
    printf("could not allocate\n");
    abort();
  }

  if (m.ptr != NULL) {
    Block *old_block = anchorage::Block::get(m.ptr);
    old_block->set_handle(nullptr);
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
    m.anchorage.flags |= ANCHORAGE_FLAG_LAZY_FREE;  // set the flag to indicate that it's free (but someone has a lock)
    printf("freed while locked. TODO!\n");

    return;
  }


  blk->set_handle(nullptr);
  m.size = 0;
  m.ptr = nullptr;
  // tell the block to coalesce the best it can
  blk->coalesce_free(*chunk);
  anchorage::barrier();
}



auto anchorage::Chunk::get(void *ptr) -> anchorage::Chunk * {
  // TODO: lock!
  for (auto *chunk : *all_chunks) {
    if (chunk->contains(ptr)) return chunk;
  }
  return nullptr;
}


auto anchorage::Chunk::contains(void *ptr) -> bool {
  return ptr >= front && ptr < tos;
}

anchorage::Chunk::Chunk(size_t pages) : pages(pages) {
  tos = front =
      (Block *)mmap(NULL, anchorage::page_size * pages, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  tos->set_next(nullptr);

  all_chunks->insert(this);
}


anchorage::Chunk::~Chunk(void) {
  munmap((void *)front, 4096 * pages);
}

anchorage::Block *anchorage::Chunk::alloc(size_t requested_size) {
  size_t size = round_up(requested_size, anchorage::block_size);
  if (size == 0) size = anchorage::block_size;
  // printf("req: %zu, get: %zu\n", requested_size, size);

  // The distance from the top_of_stack pointer to the end of the chunk
  off_t end_of_chunk = (off_t)this->front + this->pages * anchorage::page_size;
  size_t space_left = end_of_chunk - (off_t)this->tos;
  if (space_left < anchorage::size_with_overhead(size)) {
    // Not enough space left at the end of the chunk to allocate!
    return NULL;
  }

  Block *blk = tos;
  tos = (Block *)((uint8_t *)this->tos + anchorage::size_with_overhead(size));

  tos->set_next(nullptr);
  blk->set_next(tos);

  high_watermark = std::max(span(), high_watermark);

  return blk;
}

void anchorage::Chunk::free(anchorage::Block *blk) {
  return;
}


void anchorage::Chunk::dump(Block *focus, const char *message) {
  printf("%-10s ", message);
  for (auto &block : *this) {
    block.dump(false, &block == focus);
  }

  printf("span:%zu, wm:%zu", span(), high_watermark);
  printf("\n");
}



int anchorage::Chunk::sweep_freed_but_locked(void) {
  int changed = 0;

  for (auto &blk : *this) {
    if (blk.is_used() && blk.handle()->anchorage.locks <= 0) {
      if (blk.handle()->anchorage.flags & ANCHORAGE_FLAG_LAZY_FREE) {
        alaska_table_put(blk.handle());
        changed++;
        blk.set_handle(nullptr);
      }
    }
  }

  return changed;
}




size_t anchorage::Chunk::span(void) const {
  return (off_t)tos - (off_t)front;
}



void anchorage::barrier(bool force) {
  anchorage::Defragmenter defrag;
  defrag.run(*all_chunks);
}



void anchorage::allocator_init(void) {
  all_chunks = new std::unordered_set<anchorage::Chunk *>();
  new Chunk(1024);  // leak, as it will be placed in the global list.
}
void anchorage::allocator_deinit(void) {
  anchorage::barrier(true);
  printf("total memmove: %f MB\n", anchorage::moved_bytes / (float)(1024.0 * 1024.0));
}
