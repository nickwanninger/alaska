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
  new_blk->clear();
  new_blk->set_handle(nullptr);
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
  tos->clear();
  blk->clear();

  tos->set_next(nullptr);
  blk->set_next(tos);

  if (span() > high_watermark) high_watermark = span();

  return blk;
}


// add and remove blocks from the free list
void anchorage::Chunk::fl_add(Block *blk) {
  // dump(blk, "fl_add");
  auto *node = (struct list_head *)blk->data();
  list_add(node, &free_list);
}
void anchorage::Chunk::fl_del(Block *blk) {
  // dump(blk, "fl_del");
  // we basically just hope that blk is free and is in the list, since I don't want to double
  // check since that would take too long...
  list_del_init((struct list_head *)blk->data());
}




void anchorage::Chunk::free(anchorage::Block *blk) {
  // printf("FREEING BLOCK\n");
  // dump_free_list();
  // Disassociate the handle
  blk->mark_as_free(*this);

  dump(blk, "Free");

  if (blk->next() == tos) {
    tos = blk;
  } else {
    fl_add(blk);
    coalesce_free(blk);
  }


  // dump_free_list();
  // printf("\n\n\n");
}




int anchorage::Chunk::coalesce_free(Block *blk) {
  // if the previous is free, ask it to coalesce instead, as we only handle
  // left-to-right coalescing.
  if (blk->prev() && blk->prev()->is_free()) {
    return coalesce_free(blk->prev());
  }

  int changes = 0;
  Block *succ = blk->next();

  // dump(blk, "Free");

  while (succ && succ->is_free() && succ != this->tos) {
    fl_del(succ);
    blk->set_next(succ->next());
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
      fl_del(blk);
      changes++;
    }
  }

  // Remove the block from the free list and add it again. This is future-proofing in case the free
  // list becomes more complex (potentially using size-class segregated lists or something)
  // fl_del(blk);
  // fl_add(blk);

  return changes;
}



void anchorage::Chunk::dump(Block *focus, const char *message) {
  // return;
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



bool anchorage::Chunk::can_move(Block *free_block, Block *to_move) {
  // we are certain to_move has a handle associated, as we would have merged
  // all the free blocks up until to_move

  // First, verify that the handle isn't locked.
  if (to_move->is_locked()) {
    return false;
  }

  // Don't move if it would leave only enough space for a block's metadata
  if (free_block->size() - to_move->size() == anchorage::block_size) {
    return false;
  }

  // Adjacency test
  if (free_block->next() == to_move) {
    return true;
  }

  // // if the slots are at least same size, you can move them!
  // if (to_move->size() <= free_block->size()) {
  //   return true;
  // }
  return false;
}




int anchorage::Chunk::perform_move(anchorage::Block *free_block, anchorage::Block *to_move) {
  size_t to_move_size = to_move->size();
  size_t free_space = free_block->size();
  ssize_t trailing_size = free_space - to_move_size;
  (void)trailing_size;
  void *src = to_move->data();
  void *dst = free_block->data();  // place the data on `cur`
  anchorage::Block *trailing_free = (anchorage::Block *)((uint8_t *)dst + to_move_size);

  assert(free_block->is_free() && to_move->is_used());

  // grab the handle we are moving before we move, as the
  // data movement could clobber the block metadata
  alaska::Mapping *handle = to_move->handle();

  if (free_block->next() == to_move) {
    // This is the most trivial movement. If the blocks are
    // adjacent to eachother, they effectively swap places.
    // Lets say you have these blocks:
    //    |--|########|XXXX
    // After this operation it should look like this:
    //    |########|--|XXXX
    //       ^     ^ This is `trailing_free`
    //       | to_move metadata clobbered here.


    // This is the |XXXX block above.
    anchorage::Block *new_next = to_move->next();  // the next of the successor
    // Remove the old block from the free list
    // chunk.fl_del(free_block);
    // move the data
    ::memmove(dst, src, to_move_size);

    // Update the handle to point to the new location
    // (TODO: maybe make this atomic? This is where a concurrent GC would do hard work)
    handle->ptr = dst;                    // Update the handle to point to the new block
    free_block->clear();                  // Clear the old block's metadata
    free_block->set_handle(handle);       // move the handle over
    free_block->set_next(trailing_free);  // now, patch up the next of `cur`
    // to_move->set_handle(nullptr);         // patch with free block

    trailing_free->clear();
    trailing_free->set_next(new_next);
    trailing_free->set_handle(nullptr);

    // Add the trailing block to the free list
    // dump(trailing_free, "TF");
    fl_add(trailing_free);
    // coalesce_free(trailing_free);
    return 1;
  }


  return 0;
}


bool anchorage::Chunk::shift_hole(anchorage::Block **hole_ptr, ShiftDirection dir) {
  auto *hole = *hole_ptr;
  // first, check that it can shift :)
  anchorage::Block *shiftee = nullptr;
  switch (dir) {
    case ShiftDirection::Left:
      shiftee = hole->prev();
      break;

    case ShiftDirection::Right:
      shiftee = hole->next();
      break;
  }

  // We can't shift if the shiftee is locked or free
  // The reason we don't want to shift a free block is to avoid infinite loops
  // of swapping two non-coalesced holes back and forth
  if (shiftee == nullptr || shiftee->is_locked() || shiftee->is_free()) return false;



  void *src = shiftee->data();
  void *dst = hole->data();

  alaska::Mapping *handle = shiftee->handle();

  if (dir == ShiftDirection::Right) {
    fl_del(hole);

    anchorage::Block *trailing_free = (anchorage::Block *)((uint8_t *)dst + shiftee->size());

    // This is the most trivial movement. If the blocks are
    // adjacent to eachother, they effectively swap places.
    // Lets say you have these blocks:
    //    |--|########|XXXX
    // After this operation it should look like this:
    //    |########|--|XXXX
    //       ^     ^ This is `trailing_free`
    //       | to_move metadata clobbered here.


    // This is the |XXXX block above.
    anchorage::Block *new_next = shiftee->next();  // the next of the successor
    // Remove the old block from the free list
    // chunk.fl_del(free_block);
    // move the data
    ::memmove(dst, src, shiftee->size());

    // Update the handle to point to the new location
    // (TODO: maybe make this atomic? This is where a concurrent GC would do hard work)
    handle->ptr = dst;              // Update the handle to point to the new block
    hole->clear();                  // Clear the old block's metadata
    hole->set_handle(handle);       // move the handle over
    hole->set_next(trailing_free);  // now, patch up the next of `cur`
    // to_move->set_handle(nullptr);         // patch with free block

    trailing_free->clear();
    trailing_free->set_next(new_next);
    trailing_free->set_handle(nullptr);

    // Add the trailing block to the free list
    // dump(trailing_free, "TF");
    fl_add(trailing_free);
    // coalesce_free(trailing_free);
    *hole_ptr = trailing_free;
    return true;
  }

  return false;
}


void anchorage::Chunk::gather_sorted_holes(ck::vec<anchorage::Block *> &out_holes) {
  out_holes.clear();

  // gather up all the holes
  struct list_head *cur = nullptr;
  list_for_each(cur, &free_list) {
    auto blk = anchorage::Block::get((void *)cur);
    out_holes.push(blk);
  }


  // sort the list of holes
  // TODO: maybe keep track of if we need this or not...
  ::qsort(out_holes.data(), out_holes.size(), sizeof(anchorage::Block *),
      [](const void *va, const void *vb) -> int {
        auto a = (uint64_t) * (void **)va;
        auto b = (uint64_t) * (void **)vb;
        return a - b;
      });
}


// in this function, Dianoga does his work.
long anchorage::Chunk::defragment(void) {
  long old_span = span();
  // pintf("\n\n\n");

  ck::vec<anchorage::Block *> holes;
  long swaps = 0;
  long loops = 0;
  long max_holes = 0;
  while (1) {
    loops++;
    // printf("===================================================\n");
    gather_sorted_holes(holes);
    if (holes.size() > max_holes) max_holes = holes.size();

    bool changed = false;
    for (int i = 0; i < holes.size() - 1; i++) {
      // printf("%d\n", i);
      auto left = holes[i];
      if (left == nullptr) continue;
      // dump(left, "left");
      // dump(right, "right");

      while (shift_hole(&left, ShiftDirection::Right)) {
        changed = true;
        swaps++;
        dump(left, "shift");
      }

      // coalesce the left hole with the right hole, and update the vector so the coalesced hole is
      // the next hole we deal with
      coalesce_free(left);
      holes[i] = nullptr;
      holes[i + 1] = left;
    }
    if (!changed) {
      break;
    }
  }
  // printf("Swaps: %lu\n", swaps);
  // printf("Loops: %lu\n", loops);
  // printf("Max Holes: %lu\n", max_holes);

  // printf("===================================================\n");

  return 0;

  for (anchorage::Block *cur = this->front; cur != NULL && cur != this->tos; cur = cur->next()) {
    if (!cur->is_free()) continue;
    dump(cur, "Defrag");

    anchorage::Block *succ = cur->next();
    anchorage::Block *latest_can_move = NULL;
    // if there is a successor and it has no locks...
    while (succ != NULL && succ != this->tos) {
      if (succ->is_used() && can_move(cur, succ)) {
        latest_can_move = succ;
        dump(succ, "look");
        // break;
      }
      succ = succ->next();
    }

    if (latest_can_move) {
      // dump(latest_can_move, "Move");
      // remove the current from the free list
      fl_del(cur);
      perform_move(cur, latest_can_move);
      // dump(NULL);
    }
  }
  dump(nullptr, "Done");
  // printf("===================================================\n");
  //
  // printf("\n\n\n");
  return old_span - span();
}
