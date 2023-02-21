/*
 * this file is part of the alaska handle-based memory management system
 *
 * copyright (c) 2023, nick wanninger <ncw@u.northwestern.edu>
 * copyright (c) 2023, the constellation project
 * all rights reserved.
 *
 * this is free software.  you are permitted to use, redistribute,
 * and modify it as specified in the file "license".
 */

#include "./allocator.h"

#include <alaska.h>
#include <alaska/internal.h>
#include <alaska/service/anchorage.h>
#include <alaska/vec.h>
#include "./internal.h"
#include <string.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <unordered_set>
#include <assert.h>


// The global set of chunks in the allocator
static std::unordered_set<anchorage::Chunk *> *all_chunks;


static size_t total_memmoved = 0;
void tracked_memmove(void *dest, void *source, size_t sz) {
  // printf("memmove %zu\n", sz);
  total_memmoved += sz;
  (void)total_memmoved;
  memmove(dest, source, sz);
}

size_t anchorage::Block::size(void) const {
  if (this->next == NULL) return 0;
  return (off_t)this->next - (off_t)this->data();
}

void *anchorage::Block::data(void) const { return (void *)(((uint8_t *)this) + sizeof(Block)); }

anchorage::Block *anchorage::Block::get(void *ptr) {
  uint8_t *bptr = (uint8_t *)ptr;
  return (anchorage::Block *)(bptr - sizeof(anchorage::Block));
}


int anchorage::Block::coalesce_free(anchorage::Chunk &chunk) {
  int changes = 0;
  Block *succ = this->next;
  // printf("coa %p ", block);
  while (succ && succ->handle == NULL) {
    this->next = succ->next;
    succ = succ->next;
    changes += 1;
  }

  if (this->next == chunk.tos || this->next == NULL) {
    // move the top of stack down if we can.
    // There are two cases to handle here.
    if (this->handle) {
      // 1. The block before `tos` is an allocated block. In this case, we move
      //    the `tos` right to the end of the allocated block.
      void *after = ((uint8_t *)this->data() + round_up(this->handle->size, BLOCK_GRANULARITY));
      chunk.tos = (Block *)after;
      this->next = (Block *)after;
      chunk.tos->next = NULL;
      chunk.tos->handle = NULL;
      changes++;
    } else {
      // 2. The block before `tos` is a free block. This block should become `tos`
      this->next = NULL;
      chunk.tos = this;
      changes++;
    }
  }
  return changes;
}




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

  if (new_block == NULL) {
    printf("could not allocate\n");
    abort();
  }

  if (m.ptr != NULL) {
    Block *old_block = anchorage::Block::get(m.ptr);
    old_block->handle = NULL;
    size_t copy_size = m.size;
    if (size < copy_size) copy_size = size;
    memcpy(new_block->data(), m.ptr, copy_size);
    memset(m.ptr, 0xFA, m.size);
  }

  new_block->handle = &m;
  m.ptr = new_block->data();
  m.size = size;
  // printf("alloc: %p %p (%zu)", &m, m.ptr, m.size);
  // new_chunk->dump(new_block);
  return m.ptr;
}


void anchorage::free(alaska::Mapping &m, void *ptr) {
  auto *blk = anchorage::Block::get(ptr);

  if (m.anchorage.locks > 0) {
    m.anchorage.flags |= ANCHORAGE_FLAG_LAZY_FREE;  // set the flag to indicate that it's free (but someone has a lock)
    printf("freed while locked. TODO!\n");

    memset(m.ptr, 0xFA, m.size);
    return;
  }


  blk->handle = NULL;
  m.size = 0;
  m.ptr = nullptr;
  // blk->coalesce_free();
  anchorage::barrier();
}


anchorage::Chunk::Chunk(size_t pages) : pages(pages) {
  tos = front = (Block *)mmap(NULL, PGSIZE * pages, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  tos->next = NULL;

  all_chunks->insert(this);
}


anchorage::Chunk::~Chunk(void) { munmap((void *)front, 4096 * pages); }

anchorage::Block *anchorage::Chunk::alloc(size_t requested_size) {
  size_t size = round_up(requested_size, BLOCK_GRANULARITY);
  if (size == 0) size = BLOCK_GRANULARITY;
  // printf("req: %zu, get: %zu\n", requested_size, size);

  // The distance from the top_of_stack pointer to the end of the chunk
  off_t end_of_chunk = (off_t)this->front + this->pages * PGSIZE;
  off_t space_left = end_of_chunk - (off_t)this->tos;
  if (space_left < SIZE_WITH_OVERHEAD(size)) {
    // Not enough space left at the end of the chunk to allocate!
    return NULL;
  }

  Block *blk = tos;
  tos = (Block *)((uint8_t *)this->tos + SIZE_WITH_OVERHEAD(size));

  tos->next = NULL;
  blk->next = tos;

  return blk;
}

void anchorage::Chunk::free(anchorage::Block *blk) { return; }


void anchorage::Chunk::dump(Block *focus) {
  for (auto &block : *this) {
    int color = 0;
    char c = '#';
    if (block.handle) {
      if (block.handle->anchorage.flags & ANCHORAGE_FLAG_LAZY_FREE) {
        color = 35;  // purple
        c = '_';
      } else if (block.handle->anchorage.locks > 0) {
        color = 31;  // red
        c = 'X';
      } else {
        color = 32;  // green
      }

      if (block.handle->ptr != block.data()) {
        color = 33;  // yellow
        c = '?';
      }
    } else {
      color = 90;  // gray
      c = '-';
    }
    (void)c;

    if (&block == focus) {
      color = 34;
    }
    printf("\e[%dm|", color);
    ssize_t sz = block.size();
    if (sz < 0 || sz > this->pages * PGSIZE) {
      printf("BAD SIZE(%zd, %p)", sz, block.handle);
    } else {
      // printf("%zu", sz);
      for (int i = 0; i <= ((sz - BLOCK_GRANULARITY) / BLOCK_GRANULARITY); i++)
        putchar(c);
    }
    printf("\e[0m");
  }
  printf("\n");
}



int anchorage::Chunk::sweep_freed_but_locked(void) {
  int changed = 0;

  for (auto &blk : *this) {
    if (blk.handle && blk.handle->anchorage.locks <= 0) {
      if (blk.handle->anchorage.flags & ANCHORAGE_FLAG_LAZY_FREE) {
        alaska_table_put(blk.handle);
        changed++;
        blk.handle = NULL;
      }
    }
  }

  return changed;
}




bool anchorage::Chunk::can_move(Block *free_block, Block *to_move) {
  // we are certain to_move has a handle associated, as we would have merged
  // all the free blocks up until to_move
  if (to_move->handle->anchorage.locks > 0) {
    // you cannot move a locked pointer
    return false;
  }


  if (to_move->handle->anchorage.flags & ANCHORAGE_FLAG_LAZY_FREE) {
    // You can't move a handle which was freed but not unlocked.
    return false;
  }

  // return false;

  size_t to_move_size = to_move->size();
  size_t free_space = free_block->size();



  // Don't move if it would leave only enough space for a block's metadata
  if (free_space - to_move_size == BLOCK_GRANULARITY) {
    return false;
  }


  bool adjacent = free_block->next == to_move;
  if (adjacent) {
    return false;
  }

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
  alaska::Mapping *handle = to_move->handle;

  if (free_block->next == to_move) {
    return 0;
    // printf("a)\n");
    // This is the most trivial movement. If the blocks are
    // adjacent to eachother, they effectively swap places.
    // Lets say you have these blocks:
    //    |--|########|XXXX
    // After this operation it should look like this:
    //    |########|--|XXXX
    //       ^ to_move metadata clobbered here.

    // This is the |XXXX block above.
    anchorage::Block *new_next = to_move->next;  // the next of the successor
    // move the data
    tracked_memmove(dst, src, to_move->handle->size);

    // Update the handle to point to the new location
    // (TODO: maybe make this atomic? This is where a concurrent GC would do hard work)
    handle->ptr = dst;
    // move the handle over
    free_block->handle = handle;

    // now, patch up the next of `cur`
    free_block->next = trailing_free;
    // patch with free block
    to_move->handle = NULL;
    trailing_free->next = new_next;
    trailing_free->handle = NULL;
    return 1;
  } else {
    // if `to_move` is not immediately after `free_block` then we
    // need to do some different operations.
    bool need_trailing = (trailing_size >= BLOCK_GRANULARITY);
    // you need a trailing block if there is more than `BLOCK_GRANULARITY` left
    // over after you would have merged the two.


    if (need_trailing) {
      // printf("b)\n");

      // initialize the trailing space
      trailing_free->next = free_block->next;
      trailing_free->handle = NULL;
      free_block->next = trailing_free;
      // move the data
      tracked_memmove(dst, src, to_move_size);
      // update the handle
      free_block->handle = handle;
      handle->ptr = dst;

      // invalidate the handle on the moved block
      to_move->handle = NULL;
      return 1;
    } else {
      // printf("c)\n");

      memmove(dst, src, to_move_size);
      free_block->handle = handle;
      handle->ptr = dst;
      to_move->handle = NULL;
      return 1;
    }
  }


  return 0;
}


int anchorage::Chunk::compact(void) {
  // printf("start:");
  // dump();
  int changes = 0;
  // go over every block. If it is free, coalesce with next pointers or
  // move them into it's place (if they aren't locked).
  anchorage::Block *cur = this->front;
  while (cur != NULL && cur != this->tos) {
    int old_changes = changes;
    if (cur->is_free()) {
      // printf("cur:  ");
      // dump(cur);
      // if this block is unallocated, coalesce all `next` free blocks,
      // and move allocated blocks forward if you can.
      changes += cur->coalesce_free(*this);
      anchorage::Block *succ = cur->next;
      anchorage::Block *latest_can_move = NULL;
      // if there is a successor and it has no locks...
      while (succ != NULL && succ != this->tos) {
        if (succ == NULL) break;
        if (succ->is_used() && can_move(cur, succ)) {
          // changes += perform_move(cur, succ);
          // break;

          latest_can_move = succ;
        }
        succ = succ->next;
      }

      if (latest_can_move) {
        // printf("mov:  ");
        // dump(latest_can_move);
        // printf("\n");
        changes += perform_move(cur, latest_can_move);
      }
    }



    if (changes != old_changes) {
      // printf("      ");
      // dump();
    }
    cur = cur->next;
  }

  return changes;
}



void anchorage::barrier(void) {
  int moved = 0;

  for (auto *chunk : *all_chunks) {
    // printf("before: ");
    // chunk->dump();
    while (true) {
      chunk->sweep_freed_but_locked();
      int changed = chunk->compact();
      moved += changed;
      if (changed == 0) break;
    }
    // printf("      ");
    // chunk->dump();
    // printf("after:  ");
    // chunk->dump();
  }
  (void)moved;
  // printf("moved %d\n", moved);
}



void anchorage::allocator_init(void) {
  all_chunks = new std::unordered_set<anchorage::Chunk *>();
  new Chunk(1024);  // leak, as it will be placed in the global list.
}
void anchorage::allocator_deinit(void) {
  printf("total memmove: %.2f MB\n", total_memmoved / (float)(1024.0 * 1024.0));
}