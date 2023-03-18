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

#include <anchorage/Chunk.hpp>
#include <anchorage/Block.hpp>
#include <anchorage/Defragmenter.hpp>

#include <alaska.h>
#include <alaska/internal.h>
#include <alaska/service/anchorage.h>
#include <alaska/vec.h>
#include <string.h>
#include <sys/mman.h>
#include <unordered_set>
#include <assert.h>




struct anchorage_lock_frame {
  struct anchorage_lock_frame *prev;
  uint64_t count;
  void *locked[];
};

thread_local struct anchorage_lock_frame *alaska_lock_root_chain = NULL;

// The global set of chunks in the allocator
static std::unordered_set<anchorage::Chunk *> *all_chunks;

auto anchorage::Chunk::all(void) -> const std::unordered_set<anchorage::Chunk *> & {
  return *all_chunks;
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
  printf(" chunk %p - span:%zu, wm:%zu", this, span(), high_watermark);
  printf("\n");
}



int anchorage::Chunk::sweep_freed_but_locked(void) {
  int changed = 0;

  // for (auto &blk : *this) {
  //   if (blk.is_used() && blk.handle()->anchorage.locks <= 0) {
  //     if (blk.handle()->anchorage.flags & ANCHORAGE_FLAG_LAZY_FREE) {
  //       alaska_table_put(blk.handle());
  //       changed++;
  //       blk.clear_handle();
  //     }
  //   }
  // }

  return changed;
}




size_t anchorage::Chunk::span(void) const {
  return (off_t)tos - (off_t)front;
}



void anchorage::barrier(bool force) {
  printf("barrier\n");
  auto *cur = alaska_lock_root_chain;
  while (cur) {
    printf("   %p count:0x%zx prev:%p", cur, cur->count, cur->prev);

    // for (uint64_t i = 0; i < cur->count; i++) {
    //   printf(" %16llx", cur->locked[i]);
    //   // printf(" %p", cur->locked[i]);
    // }
    printf("\n");
    cur = cur->prev;
  }
  // return;
  // printf("--- begin defrag\n");
  // for (auto *chunk : anchorage::Chunk::all()) {
  // 	printf("chunk %p\n", chunk);
  // }
  anchorage::Defragmenter defrag;
  defrag.run(*all_chunks);
  // printf("--- end defrag\n");
}



void anchorage::allocator_init(void) {
  all_chunks = new std::unordered_set<anchorage::Chunk *>();
  new Chunk(anchorage::min_chunk_pages);  // leak, as it will be placed in the global list.
}
void anchorage::allocator_deinit(void) {
  anchorage::barrier(true);
  printf(
      "total memmove: %f MB (%lu bytes)\n", anchorage::moved_bytes / (float)(1024.0 * 1024.0), anchorage::moved_bytes);
}
