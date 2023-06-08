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

#include <alaska/alaska.hpp>
#include <alaska/barrier.hpp>

#include <ck/set.h>

#include <string.h>
#include <sys/mman.h>
#include <assert.h>
#include <unistd.h>



// The global set of chunks in the allocator
static ck::set<anchorage::Chunk *> *all_chunks;

auto anchorage::Chunk::all(void) -> const ck::set<anchorage::Chunk *> & {
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

anchorage::Chunk::Chunk(size_t pages)
    : pages(pages) {
  size_t size = anchorage::page_size * pages;
  tos = front = (Block *)mmap(
      NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);

  madvise(tos, size, MADV_HUGEPAGE);
  printf("allocated %f Mb for a chunk\n", (size) / 1024.0 / 1024.0);
  tos->set_next(nullptr);

  all_chunks->add(this);
}

anchorage::Chunk::~Chunk(void) {
  munmap((void *)front, 4096 * pages);
}

anchorage::Block *anchorage::Chunk::alloc(size_t requested_size) {
  size_t size = round_up(requested_size, anchorage::block_size);
  if (size == 0) size = anchorage::block_size;

  // The distance from the top_of_stack pointer to the end of the chunk
  off_t end_of_chunk = (off_t)this->front + this->pages * anchorage::page_size;
  size_t space_left = end_of_chunk - (off_t)this->tos - block_size;
  if (space_left <= anchorage::size_with_overhead(size)) {
    // Not enough space left at the end of the chunk to allocate!
    return NULL;
  }

  Block *blk = tos;
  tos = (Block *)((uint8_t *)this->tos + anchorage::size_with_overhead(size));

  tos->set_next(nullptr);
  blk->set_next(tos);

  if (span() > high_watermark) high_watermark = span();

  return blk;
}

void anchorage::Chunk::free(anchorage::Block *blk) {
  // Disassociate the handle
  blk->mark_as_free(*this);
  return;
}


void anchorage::Chunk::dump(Block *focus, const char *message) {
  printf("%-10s ", message);
  for (auto &block : *this) {
    block.dump(false, &block == focus);
  }
  printf("\n");
}



int anchorage::Chunk::sweep_freed_but_locked(void) {
  int changed = 0;
  return changed;
}


size_t anchorage::Chunk::span(void) const {
  return (off_t)tos - (off_t)front;
}




static long last_accessed = 0;
extern "C" void anchorage_chunk_dump_usage(alaska::Mapping *m) {
  alaska::barrier::begin();

  long just_accessed = (long)m->ptr;
  if (last_accessed == 0) last_accessed = just_accessed;
  long stride = just_accessed - last_accessed;
  last_accessed = just_accessed;

  auto *c = anchorage::Chunk::get(m->ptr);
  if (c != NULL && stride != 0) {
    auto b = anchorage::Block::get(m->ptr);
    c->dump(b, "access");
  }
  alaska::barrier::end();
}


void anchorage::allocator_init(void) {
  all_chunks = new ck::set<anchorage::Chunk *>();
}

void anchorage::allocator_deinit(void) {
}