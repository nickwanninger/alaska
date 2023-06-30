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
    : pages(pages)
    , free_list(*this) {
  size_t size = anchorage::page_size * pages;
  tos = front = (Block *)mmap(
      NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);

  madvise(tos, size, MADV_HUGEPAGE);
  printf(
      "allocated %f Mb for a chunk (%p-%p)\n", (size) / 1024.0 / 1024.0, tos, (uint64_t)tos + size);
  tos->set_next(nullptr);
  all_chunks->add(this);
}

anchorage::Chunk::~Chunk(void) {
  munmap((void *)front, 4096 * pages);
}


// Split a free block, leaving `to_split` in the free list.
bool anchorage::Chunk::split_free_block(anchorage::Block *to_split, size_t required_size) {

	printf("\nSPLIT\n");
  auto current_size = to_split->size();
  if (current_size == required_size) return true;
	to_split->dump_content("to split");


  size_t remaining_size = current_size - required_size;

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




// add and remove blocks from the free list
void anchorage::Chunk::fl_add(Block *blk) {
  // blk->dump_content("before add");
  free_list.add(blk);
  // blk->dump_content("after add");
}
void anchorage::Chunk::fl_del(Block *blk) {
  blk->dump_content("before del");
  free_list.remove(blk);
  // blk->dump_content("after del");
}




anchorage::Block *anchorage::Chunk::alloc(size_t requested_size) {
  // dump(NULL, "Before");
  size_t size = round_up(requested_size, anchorage::block_size);
  if (size == 0) size = anchorage::block_size;

  if (auto blk = free_list.search(size); blk != nullptr) {
    if (split_free_block(blk, size)) {
      active_bytes += blk->size();
      // No need to remove `blk` from the list, as free_list::search does that for us.
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

  // dump(blk, "Alloc");

  active_bytes += blk->size();
  return blk;
}



void anchorage::Chunk::free(anchorage::Block *blk) {
  active_bytes -= blk->size();
  // printf("frag ratio %f - %zu allocated, %zu used\n", span() / (float)active_bytes, active_bytes,
  // span()); Disassociate the handle
  blk->mark_as_free(*this);
  // blk->dump_content("marked");
  fl_add(blk);
  coalesce_free(blk);
  printf("frag: %-10.6f\n", span() / (double)active_bytes);
  // dump(blk, "Free");
}

size_t anchorage::Chunk::span(void) const {
  return (off_t)tos - (off_t)front;
}

////////////////////////////////////////////////////////////////////////////////

int anchorage::Chunk::coalesce_free(Block *blk) {
  // if the previous is free, ask it to coalesce instead, as we only handle
  // left-to-right coalescing.
  while (blk->prev() && blk->prev()->is_free()) {
    blk = blk->prev();
  }

  int changes = 0;
  Block *succ = blk->next();

  while (succ && succ->is_free() && succ != this->tos) {
    // If the successor is free, remove it from the free list
    // and take over it's space.
    fl_del(succ);
    blk->set_next(succ->next());
    succ = succ->next();
    changes += 1;
  }

  // If the next node is the top of stack, this block should become top of stack
  if (true && (blk->next() == tos || blk->next() == NULL)) {
    // move the top of stack down if we can.
    // There are two cases to handle here.

    if (blk->is_used()) {
      // 1. The block before `tos` is an allocated block. In this case, we move
      //    the `tos` right to the end of the allocated block.
      auto after = blk->next();  // TODO: might not work!
      tos = (Block *)after;
      blk->set_next((Block *)after);
      tos->set_next(NULL);
      tos->mark_as_free(*this);
      changes++;
      fprintf(stderr, "Edge case check: blk is used but it is being coalesced? TF?\n");
      abort();
    } else {
      // 2. The block before `tos` is a free block. This block should become `tos`
      blk->set_next(NULL);
      tos = blk;
      fl_del(blk);
      changes++;
    }
  } else {
    // If the next block is *not* the top of stack, remove and re-add to the free list. This ensures
    // that if the block changed size, the free list understands that and puts it in the right place
    fl_del(blk);
    fl_add(blk);
  }

  // dump(blk, "COAL");

  return changes;
}

static char *readable_fs(double size /*in bytes*/, char *buf) {
  int i = 0;
  const char *units[] = {"B", "kB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"};
  while (size > 1024) {
    size /= 1024;
    i++;
  }
  sprintf(buf, "%.*f %s", i * 2, size, units[i]);
  return buf;
}

void anchorage::Chunk::dump(Block *focus, const char *message) {
  return;
  printf("%-10s ", message);


  char buf[32];
  printf("active: %-10s ", readable_fs(active_bytes, buf));
  printf("span: %-10s ", readable_fs(span(), buf));
  printf("frag: %-10.6f", span() / (double)active_bytes);
  // printf("\n");
  // return;

  for (auto &block : *this) {
    block.dump(false, &block == focus);
  }

  printf("\n");
}

void anchorage::Chunk::dump_free_list(void) {
  printf("Free list:\n");
  ck::vec<anchorage::Block *> blocks;
  free_list.collect(blocks);

  for (auto *blk : blocks) {
    blk->dump_content();
    // dump(blk, "FL");
  }
  printf("\n");
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
    printf("\n");
    shiftee->dump_content("Shiftee");
    hole->dump_content("Hole");

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

    hole->dump_content("after_move");
    trailing_free->dump_content("trailing_free");
    // coalesce_free(trailing_free);
    *hole_ptr = trailing_free;
    return true;
  }

  return false;
}


void anchorage::Chunk::gather_sorted_holes(ck::vec<anchorage::Block *> &out_holes) {
  out_holes.clear();

  free_list.collect(out_holes);

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
  printf("DEFRAG\n");

  dump(nullptr, "Before");
  ck::vec<anchorage::Block *> holes;
  long swaps = 0;
  long max_holes = 0;

  long free_blocks_before = free_list.size();

  auto start = alaska_timestamp();

  // The defragmentation routine in anchorage is relatively simple. At it's core, it
  // operates in two steps. First, it attempts to merge as many free blocks as it can,
  // within reason. This is done by shifting them closer together iteratively. The goal
  // is to have less, larger, holes after the first while loop is complete. Second, it
  // attempts to swap those holes with values on the end of the heap.
  // This is done by iterating over each of the holes, in the free list,
  while (1) {
    gather_sorted_holes(holes);
    if (holes.size() > max_holes) max_holes = holes.size();

    bool changed = false;
    for (int i = 0; i < holes.size() - 1; i++) {
      auto left = holes[i];
      if (left == nullptr) continue;



      while (true) {
        left->dump_content("before shift");
        if (!shift_hole(&left, ShiftDirection::Right)) {
          break;
        }
        left->dump_content("after shift");
        changed = true;
        swaps++;
        dump(left, "swap");
      }

      // coalesce the left hole with the right hole, and update the vector so the coalesced hole
      // is the next hole we deal with
      coalesce_free(left);
      holes[i] = nullptr;
      holes[i + 1] = left;
    }

    if (!changed) break;
  }


  // gather_sorted_holes(holes);
  // for (auto *hole : holes) {
  //   // printf("hole: %p\n", hole);
  //
  //   // printf("Part 2:\n");
  //   // Can `cur` fit into `hole`?
  //   for (auto *cur = tos->prev(); cur != NULL && cur != hole; cur = cur->prev()) {
  //     if (hole->is_used()) break;
  //     if (cur->is_free() || cur->is_locked()) continue;
  //
  //     auto cursize = cur->size();
  //     auto holesize = hole->size();
  //
  //     if (cursize < holesize) {
  //       if (split_free_block(hole, cursize)) {
  //         dump(hole, "hole");
  //         dump(cur, "cur");
  //         fl_del(hole);
  //
  //         memmove(hole->data(), cur->data(), cursize);
  //         auto *handle = cur->handle();
  //         handle->ptr = hole->data();
  //         hole->set_handle(handle);
  //         cur->set_handle(nullptr);
  //         hole = hole->next();
  //
  //
  //         // `cur` has now become a free block, so we have to add it to the freelist and coalesce
  //         it fl_add(cur); coalesce_free(cur);
  //       }
  //     } else {
  //       // The hole is exactly big enough for the allocation
  //       break;
  //     }
  //   }
  // }

  long end = alaska_timestamp();
  (void)(end - start);  // use so I can comment out the logging below

  long block_count = 0;
  for (anchorage::Block *cur = this->front; cur != NULL && cur != this->tos; cur = cur->next()) {
    block_count++;
  }

  long saved = old_span - span();
  long free_blocks_after = free_list.size();
  (void)free_blocks_before;
  (void)free_blocks_after;

  dump(NULL, "After");
  // char buf0[32];
  // char buf1[32];
  //
  // printf("save:%-10s swps:%-10lu ms:%-10f total:%-10s blocks:%-10ld free_blocks:%ld->%ld
  // (%ld)\n",
  //     readable_fs(saved, buf0), swaps, (end - start) / 1000.0 / 1000.0, readable_fs(span(),
  //     buf1), block_count, free_blocks_before, free_blocks_after, free_blocks_after -
  //     free_blocks_before);


  return saved;
}
