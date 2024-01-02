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

#include <alaska/alaska.hpp>
#include <alaska/barrier.hpp>

#include <ck/set.h>

#include <string.h>
#include <sys/mman.h>
#include <unistd.h>



static constexpr bool do_dumps = true;

anchorage::Chunk *anchorage::Chunk::to_space = NULL;
anchorage::Chunk *anchorage::Chunk::from_space = NULL;

void anchorage::Chunk::swap_spaces() {
  auto old_from = from_space;
  from_space = to_space;
  to_space = old_from;
}

static void alldump(anchorage::Block *focus, const char *message) {
  if constexpr (do_dumps) {
    printf("%-10s | ", message);
    anchorage::Chunk::to_space->dump(focus, "to");
    printf("%-10s | ", "");
    anchorage::Chunk::from_space->dump(focus, "from");
  }
}

auto anchorage::Chunk::get(void *ptr) -> anchorage::Chunk * {
  // TODO: lock!
  if (to_space->contains(ptr)) return to_space;
  if (from_space->contains(ptr)) return from_space;
  return nullptr;
}


auto anchorage::Chunk::contains(void *ptr) -> bool {
  return ptr >= front && ptr < tos;
}

anchorage::Chunk::Chunk(size_t pages)
    : pages(pages)
    , free_list(*this) {
  size_t size = anchorage::page_size * pages;
  front = (Block *)mmap(
      NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);

  printf("heap @ %p\n", front);

  front->mark_locked(true);
  front->set_handle((alaska::Mapping *)-1);  // Intentionally set an invalid handle
  tos = front + 2;
  front->set_next(tos);

  tos->set_next(nullptr);
}

anchorage::Chunk::~Chunk(void) {
  munmap((void *)front, 4096 * pages);
}


// Split a free block, leaving `to_split` in the free list.
bool anchorage::Chunk::split_free_block(anchorage::Block *to_split, size_t required_size) {
  auto current_size = to_split->size();
  if (current_size == required_size) return true;

  size_t remaining_size = current_size - required_size;

  // If there isn't enough space in the leftover block for data, don't split the block.
  // TODO: this is a gross kind of fragmentation that I dont quite know how to deal with yet.
  if (remaining_size < anchorage::block_size * 3) {
    return true;
  }

  auto *new_blk = (anchorage::Block *)((char *)to_split->data() + required_size);
  new_blk->clear();
  new_blk->set_handle(nullptr);
  new_blk->set_next(to_split->next());
  to_split->set_next(new_blk);


  fl_add(new_blk);

  return true;
}




// add and remove blocks from the free list
void anchorage::Chunk::fl_add(Block *blk) {
  ALASKA_ASSERT(blk < tos, "A block being added to the free list must be below tos");
  // blk->dump_content("before add");
  free_list.add(blk);
  // blk->dump_content("after add");
}
void anchorage::Chunk::fl_del(Block *blk) {
  // blk->dump_content("before del");
  free_list.remove(blk);
  // blk->dump_content("after del");
}




anchorage::Block *anchorage::Chunk::alloc(size_t requested_size) {
  validate_heap("alloc - start");

  size_t size = round_up(requested_size, anchorage::block_size);
  if (size == 0) size = anchorage::block_size;

  if (auto blk = free_list.search(size); blk != nullptr) {
    if (split_free_block(blk, size)) {
      active_bytes += blk->size();
      active_blocks++;
      // printf("alloc return %p from free list!\n", blk);
      // No need to remove `blk` from the list, as free_list::search does that for us.
      blk->set_handle((alaska::Mapping *)-1);
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

  validate_heap("alloc - before bump");

  // Grab the top-of-stack. We are bump allocating
  Block *blk = tos;
  // Shift the top-of-stack.
  tos = (Block *)((uint8_t *)this->tos + anchorage::size_with_overhead(size));
  tos->clear();
  blk->clear();

  tos->set_next(nullptr);
  tos->set_prev(blk);
  blk->set_next(tos);
  blk->set_handle((alaska::Mapping *)-1);
  validate_heap("alloc - after bump");

  if (span() > high_watermark) high_watermark = span();

  active_bytes += blk->size();
  active_blocks++;
  // printf("alloc return %p\n", blk);
  return blk;
}



void anchorage::Chunk::free(anchorage::Block *blk) {
  validate_heap("free - start");
  ALASKA_ASSERT(blk < tos, "A live block must be below the top of stack");
  active_bytes -= blk->size();
  active_blocks--;
  // printf("frag ratio %f - %zu allocated, %zu used\n", span() / (float)active_bytes, active_bytes,
  // span()); Disassociate the handle
  blk->mark_as_free(*this);
  // blk->dump_content("marked");
  fl_add(blk);

  validate_heap("free - before coalesce_free, after fl_add");
  coalesce_free(blk);
  validate_heap("free - after coalesce_free");
}

size_t anchorage::Chunk::span(void) const {
  return (off_t)tos - (off_t)front;
}

////////////////////////////////////////////////////////////////////////////////

int anchorage::Chunk::coalesce_free(Block *blk) {
  // if the previous is free, ask it to coalesce instead, as we only handle
  // left-to-right coalescing.
  while (true) {
    if (blk == this->front) break;
    ALASKA_ASSERT(blk->prev() != NULL, "Previous block must not be null");
    // If the previous block isn't free, break. we are at the start of a run of free blocks
    if (!blk->prev()->is_free()) break;
    blk = blk->prev();
  }

  int changes = 0;
  Block *succ = blk->next();
  while (succ && succ->is_free() && succ != this->tos) {
    ALASKA_ASSERT(succ > blk, "successor is after cur");
    ALASKA_ASSERT(succ < tos, "successor must be before top of stack");
    // If the successor is free, remove it from the free list
    // and take over it's space.
    fl_del(succ);
    blk->set_next(succ->next());
    succ = succ->next();
    changes += 1;
  }

  fl_del(blk);
  if (blk->next() == tos) {
    this->tos = blk;
    blk->set_next(NULL);  // tos has null as next
  } else {
    fl_add(blk);
  }

  // if (changes > 0) printf("changes %d\n", changes);
  return changes;
}

void anchorage::Chunk::dump(Block *focus, const char *message) {
  if constexpr (do_dumps) {
    printf("%-10s ", message);
    for (auto &block : *this) {
      block.dump(false, &block == focus);
    }

    printf("\n");
  }
}

void anchorage::Chunk::dump_free_list(void) {
  return;
  printf("Free list:\n");
  ck::vec<anchorage::Block *> blocks;
  free_list.collect(blocks);

  for (auto *blk : blocks) {
    blk->dump_content();
  }
  printf("\n");
}


void anchorage::allocator_init(void) {
  anchorage::Chunk::to_space = new anchorage::Chunk(anchorage::min_chunk_pages);
  anchorage::Chunk::from_space = new anchorage::Chunk(anchorage::min_chunk_pages);
}

void anchorage::allocator_deinit(void) {
}



void anchorage::Chunk::validate_block(anchorage::Block *blk, const char *context_name) {
  if (blk->prev() == NULL)
    ALASKA_ASSERT(blk == front, "if prev() is null, the block must be the bottom of the stack");
  if (blk->next() == NULL)
    ALASKA_ASSERT(blk == tos, "if next() is null, the block must be top of stack");

  if (blk->next() != NULL)
    ALASKA_ASSERT(blk->next()->prev() == blk, "The next's previous must be this block");
  if (blk->prev() != NULL)
    ALASKA_ASSERT(blk->prev()->next() == blk, "The prev's next must be this block");
}

void anchorage::Chunk::validate_heap(const char *context_name) {
  return;
  // printf("Validating in context '%s'\n", context_name);
  // dump(nullptr, "Validate");
  ck::vec<anchorage::Block *> v_all_free_blocks;
  free_list.collect(v_all_free_blocks);
  ck::set<anchorage::Block *> free_blocks;
  for (auto *b : v_all_free_blocks) {
    free_blocks.add(b);
  }


  // dump(NULL, "Heap");


  for (auto &b : *this) {
    if (b.is_free()) {
      ALASKA_ASSERT(free_blocks.contains(&b), "A free block must be in the free list");
    }
    // dump(&b, "checking");
  }
}


// In this function, Dianoga does his work.
long anchorage::Chunk::perform_compaction(
    anchorage::Chunk &dst_space, anchorage::CompactionConfig &config) {
  // How much free space is there in the heap at the start?
  long used_start = memory_used_including_overheads();
  // How many tokens have been spent so far?
  unsigned long tokens_spent = 0;

  // Returns true if you are out of tokens.
  auto out_of_tokens = [&]() -> bool {
    // return false;
    return tokens_spent >= config.available_tokens;
  };
  // spend a token and return if you are out.
  auto spend_tokens = [&](long count) -> bool {
    tokens_spent += count;
    return out_of_tokens();
  };



  auto migrate_blk = [&](anchorage::Block *blk) {
    // printf("migrate %p %zu\n", blk, blk->size());
    auto new_blk = dst_space.alloc(blk->size());
    ALASKA_ASSERT(new_blk != NULL, "Could not allocate a block in the to space");

    // Move the data
    memcpy(new_blk->data(), blk->data(), blk->size());
    memset(blk->data(), 0xFF, blk->size());
    // Spend tokens to track we moved this object
    spend_tokens(blk->size());
    // Patch the handle
    new_blk->set_handle(blk->handle());
    auto *handle = blk->handle();
    handle->set_pointer(new_blk->data());

    this->free(blk);
  };


  for (auto *cur = tos->prev(); cur != NULL; cur = cur->prev()) {
    if (cur->is_free()) {
      continue;
    }

    if (cur->is_locked()) {
      continue;
    }

    // if (cur->size() > config.available_tokens - tokens_spent) {
    //   continue;
    // }

    migrate_blk(cur);
    if (out_of_tokens()) break;
  }



  if (not out_of_tokens()) {
    struct list_head *lists[anchorage::FirstFitSegFreeList::num_free_lists];
    free_list.collect_freelists(lists);
    bool hit_end = false;
    for (int i = anchorage::FirstFitSegFreeList::num_free_lists - 1; i >= 0; i--) {
      if (hit_end) break;
      struct list_head *cur = nullptr;
      list_for_each(cur, lists[i]) {
        auto blk = anchorage::Block::get((void *)cur);

        off_t start = (off_t)(cur + 1);
        off_t end = (off_t)blk->data() + blk->size();

        start = round_up(start, 0x1000);
        end = round_down(end, 0x1000);

        if (end - start < 10 * 4096) {
          hit_end = true;
          continue;
        }

        madvise((void *)start, end - start, MADV_DONTNEED);
      }
    }
  }


  long end_saved = high_watermark - span();
  size_t saved_pages = end_saved / 4096;
  if (saved_pages > 2) {
    // start clearing data *after* the top of stack. This avoid the case when the tos
    // just happens to be at offset zero on a page, and keesp it from being "corrupted"
    // (set to zero) by a MADV_DONTNEED call.
    uint64_t dont_need_start = (uint64_t)(tos->data());
    madvise((void *)round_up(dont_need_start, 4096), 1 + saved_pages * 4096, MADV_DONTNEED);

    high_watermark = span();
  }



  long used_end = memory_used_including_overheads();
  return used_start - used_end;
}
