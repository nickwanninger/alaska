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
  // for (auto &block : *this) {
  //   block.dump(false, &block == focus);
  // }

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


bool anchorage::Chunk::can_move(Block *free_block, Block *to_move) {
  // we are certain to_move has a handle associated, as we would have merged
  // all the free blocks up until to_move
  if (to_move->handle()->anchorage.locks > 0) {
    // you cannot move a locked pointer
    return false;
  }


  if (to_move->handle()->anchorage.flags & ANCHORAGE_FLAG_LAZY_FREE) {
    // You can't move a handle which was freed but not unlocked.
    return false;
  }

  // return false;

  size_t to_move_size = to_move->size();
  size_t free_space = free_block->size();


  // Don't move if it would leave only enough space for a block's metadata
  if (free_space - to_move_size == anchorage::block_size) {
    return false;
  }

  // bool adjacent = free_block->next() == to_move;
  // if (adjacent) {
  //   return true;
  // }

  // if the slots are the same size, you can move them!
  if (to_move_size <= free_space) {
    return true;
  }
  return false;
}


int anchorage::Chunk::perform_move(anchorage::Block *free_block, anchorage::Block *to_move) {
  size_t to_move_size = to_move->size();
  size_t free_space = free_block->size();
  ssize_t trailing_size = free_space - to_move_size;
  void *src = to_move->data();
  void *dst = free_block->data();  // place the data on `cur`
  anchorage::Block *trailing_free = (anchorage::Block *)((uint8_t *)dst + to_move_size);


  assert(free_block->is_free() && to_move->is_used());


  // grab the handle we are moving before we move, as the
  // data movement could clobber the block metadata
  alaska::Mapping *handle = to_move->handle();

  if (free_block->next() == to_move) {
    // return 0;
    // printf("a)\n");
    // This is the most trivial movement. If the blocks are
    // adjacent to eachother, they effectively swap places.
    // Lets say you have these blocks:
    //    |--|########|XXXX
    // After this operation it should look like this:
    //    |########|--|XXXX
    //       ^ to_move metadata clobbered here.

    // This is the |XXXX block above.
    anchorage::Block *new_next = to_move->next();  // the next of the successor
    // move the data
    anchorage::memmove(dst, src, handle->size);

    // Update the handle to point to the new location
    // (TODO: maybe make this atomic? This is where a concurrent GC would do hard work)
    handle->ptr = dst;
    free_block->clear();
    // move the handle over
    free_block->set_handle(handle);
    // now, patch up the next of `cur`
    free_block->set_next(trailing_free);
    // patch with free block
    to_move->set_handle(nullptr);

    trailing_free->clear();
    trailing_free->set_next(new_next);
    trailing_free->set_handle(nullptr);
    return 1;
  } else {
    // if `to_move` is not immediately after `free_block` then we
    // need to do some different operations.
    bool need_trailing = (trailing_size >= (ssize_t)anchorage::block_size);
    // you need a trailing block if there is more than `anchorage::block_size` left
    // over after you would have merged the two.


    if (need_trailing) {
      // printf("b)\n");

      // initialize the trailing space
      trailing_free->set_next(free_block->next());
      trailing_free->set_handle(nullptr);
      free_block->set_next(trailing_free);
      // move the data
      anchorage::memmove(dst, src, to_move_size);
      // update the handle
      free_block->set_handle(handle);
      handle->ptr = dst;

      // invalidate the handle on the moved block
      to_move->set_handle(nullptr);
      return 1;
    } else {
      // printf("c)\n");

      memmove(dst, src, to_move_size);
      free_block->clear();
      free_block->set_handle(handle);
      handle->ptr = dst;
      to_move->clear();
      to_move->set_handle(nullptr);
      return 1;
    }
  }


  return 0;
}


int anchorage::Chunk::compact(void) {
  int changes = 0;
  // go over every block. If it is free, coalesce with next pointers or
  // move them into it's place (if they aren't locked).
  anchorage::Block *cur = this->front;
  while (cur != NULL && cur != this->tos) {
    int old_changes = changes;
    if (cur->is_free()) {
      // if this block is unallocated, coalesce all `next` free blocks,
      // and move allocated blocks forward if you can.
      cur->coalesce_free(*this);
      anchorage::Block *succ = cur->next();
      anchorage::Block *latest_can_move = NULL;
      // if there is a successor and it has no locks...
      while (succ != NULL && succ != this->tos) {
        if (succ->is_used() && can_move(cur, succ)) {
          latest_can_move = succ;
          break;
        }
        succ = succ->next();
      }

      if (latest_can_move) {
        changes += perform_move(cur, latest_can_move);
      }
    }


    if (changes != old_changes) {
      // dump(cur, "Move");
      // printf("      ");
      // dump();
    }
    cur = cur->next();
  }


  // ssize_t pages_till_wm = (high_watermark - span()) / anchorage::page_size;
  // printf("pages: %zd\n", pages_till_wm);
  // TODO: MADV_DONTNEED those leftover pages if they are beyond a certain point
  return changes;
}


void anchorage::barrier(bool force) {
  long last_moved = anchorage::moved_bytes;

  printf("====================== BARRIER ======================\n");

  for (auto *chunk : *all_chunks) {
    chunk->dump(nullptr, "Before");

    // for (auto &blk : *chunk) {
    //   blk.dump(true);
    //   printf("\n");
    // }
    while (true) {
      chunk->sweep_freed_but_locked();
      auto changed = chunk->compact();
      if (changed == 0) break;
      // chunk->dump(nullptr, "After");
    }
    // for (auto &blk : *chunk) {
    //   blk.dump(true);
    //   printf("\n");
    // }
  }
  (void)last_moved;
  // printf("moved %ld bytes (total: %ld)\n", total_memmoved - last_moved, total_memmoved);
}



void anchorage::allocator_init(void) {
  all_chunks = new std::unordered_set<anchorage::Chunk *>();
  new Chunk(1024);  // leak, as it will be placed in the global list.
}
void anchorage::allocator_deinit(void) {
  anchorage::barrier(true);
  printf("total memmove: %f MB\n", anchorage::moved_bytes / (float)(1024.0 * 1024.0));
}
