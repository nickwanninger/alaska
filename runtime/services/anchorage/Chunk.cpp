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
  free_list = LIST_HEAD_INIT(free_list);

  all_chunks->add(this);
}

anchorage::Chunk::~Chunk(void) {
  munmap((void *)front, 4096 * pages);
}


// Split a free block, leaving `to_split` in the free list.
bool anchorage::Chunk::split_free_block(anchorage::Block *to_split, size_t required_size) {
  // printf("Attempting to split block of size %zu for %zu required size\n", to_split->size(),
  //     required_size);
  // dump(to_split, "To Split");


  auto current_size = to_split->size();
  if (current_size == required_size) return true;


  size_t remaining_size = current_size - required_size;


  // printf("remaining: %zu\n", remaining_size);

  // If there isn't enough space in the leftover block for data, don't split the block.
  // TODO: this is a gross kind of fragmentation that I dont quite know how to deal with yet.
  if (remaining_size < anchorage::block_size * 2) {
    return true;
  }

  auto *new_blk = (anchorage::Block *)((char *)to_split->data() + required_size);
  new_blk->set_next(to_split->next());
  new_blk->set_prev(to_split);
  to_split->set_next(new_blk);
  new_blk->next()->set_prev(to_split);


  fl_add(new_blk);

  return true;
}

anchorage::Block *anchorage::Chunk::alloc(size_t requested_size) {
  size_t size = round_up(requested_size, anchorage::block_size);
  if (size == 0) size = anchorage::block_size;

  // first, check the free list :)
  struct list_head *cur = nullptr;
  list_for_each(cur, &free_list) {
    auto blk = anchorage::Block::get((void *)cur);
    if (blk->size() >= size && split_free_block(blk, size)) {
      fl_del(blk);
      return blk;
    }
  }

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


// add and remove blocks from the free list
void anchorage::Chunk::fl_add(Block *blk) {
  auto *node = (struct list_head *)blk->data();
  list_add(node, &free_list);
}
void anchorage::Chunk::fl_del(Block *blk) {
  // we basically just hope that blk is free and is in the list, since I don't want to double
  // check since that would take too long...
  list_del_init((struct list_head *)blk->data());
}

void anchorage::Chunk::free(anchorage::Block *blk) {
  printf("================================= FREEING BLOCK =================================\n");
  dump_free_list();
  // Disassociate the handle
  blk->mark_as_free(*this);

  if (blk->next() == tos) {
    tos = blk;
  } else {
    fl_add(blk);
  }

  coalesce_free(blk);


  dump_free_list();
  printf("\n\n\n");
  return;
}


int anchorage::Chunk::coalesce_free(Block *blk) {
  // if the previous is free, ask it to coalesce instead, as we only handle
  // left-to-right coalescing.
  if (blk->prev() && blk->prev()->is_free()) {
    return coalesce_free(blk->prev());
  }

  int changes = 0;
  Block *succ = blk->next();

  dump(blk, "Free");

  while (succ && succ->is_free()) {
    blk->set_next(succ->next());

    fl_del(succ);
    succ = succ->next();
    changes += 1;
  }

  // If the next node is the top of stack, this block should become top of stack
  if (blk->next() == tos || blk->next() == NULL) {
    // move the top of stack down if we can.
    // There are two cases to handle here.

    if (blk->is_used()) {
      // 1. The block before `tos` is an allocated block. In this case, we move
      //    the `tos` right to the end of the allocated block.
      // void *after = ((uint8_t *)data() + round_up(handle()->size, anchorage::block_size));
      auto after = blk->next();  // TODO: might not work!
      tos = (Block *)after;
      blk->set_next((Block *)after);
      tos->set_next(NULL);
      tos->mark_as_free(*this);
      changes++;
    } else {
      // 2. The block before `tos` is a free block. This block should become `tos`
      blk->set_next(NULL);
      tos = blk;
      changes++;
    }
  }

  // Remove the block from the free list and add it again. This is future-proofing in case the free
  // list becomes more complex (potentially using size-class segregated lists or something)
	fl_del(blk);
	fl_add(blk);

  return changes;
}

void anchorage::Chunk::dump(Block *focus, const char *message) {
  printf("%-10s ", message);
  for (auto &block : *this) {
    block.dump(false, &block == focus);
  }
  printf("\n");
}

void anchorage::Chunk::dump_free_list(void) {
  printf("Free list:\n");
  struct list_head *cur = nullptr;
  list_for_each(cur, &free_list) {
    auto blk = anchorage::Block::get((void *)cur);
    dump(blk, "FL");
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
