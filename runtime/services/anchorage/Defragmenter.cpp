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


#include <alaska/internal.h>
#include <assert.h>
#include <anchorage/Anchorage.hpp>
#include <anchorage/Defragmenter.hpp>
#include <anchorage/Chunk.hpp>
#include <anchorage/Block.hpp>

bool anchorage::Defragmenter::can_move(Block *free_block, Block *to_move) {
  // we are certain to_move has a handle associated, as we would have merged
  // all the free blocks up until to_move

  // First, verify that the handle isn't locked.
  // if (to_move->handle()->anchorage.locks > 0) return false;

  // Second, check if the handle is one of the buggy handles that was freed before it was unlocked
  // if (to_move->handle()->anchorage.flags & ANCHORAGE_FLAG_LAZY_FREE) return false;


  // Don't move if it would leave only enough space for a block's metadata
  if (free_block->size() - to_move->size() == anchorage::block_size) {
    return false;
  }

  // Adjacency test
  if (free_block->next() == to_move) {
    // return true;
  }

  // if the slots are at least same size, you can move them!
  if (to_move->size() <= free_block->size()) {
    return true;
  }
  return false;
}

int anchorage::Defragmenter::perform_move(anchorage::Block *free_block, anchorage::Block *to_move) {
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
    anchorage::memmove(dst, src, to_move_size);

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
    return 1;
  } else {
    // if `to_move` is not immediately after `free_block` then we
    // need to do some different operations.
    bool need_trailing = (trailing_size >= (ssize_t)anchorage::block_size);
    // you need a trailing block if there is more than `anchorage::block_size` left
    // over after you would have merged the two.


    if (need_trailing) {
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
      anchorage::memmove(dst, src, to_move_size);
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


int anchorage::Defragmenter::naive_compact(anchorage::Chunk &chunk) {
  int changes = 0;
  // go over every block. If it is free, coalesce with next pointers or
  // move them into it's place (if they aren't locked).
  anchorage::Block *cur = chunk.front;
  while (cur != NULL && cur != chunk.tos) {
    int old_changes = changes;
    // chunk.dump(cur);

    if (cur->is_free()) {
      // if this block is unallocated, coalesce all `next` free blocks,
      // and move allocated blocks forward if you can.
      cur->coalesce_free(chunk);
      anchorage::Block *succ = cur->next();
      anchorage::Block *latest_can_move = NULL;
      // if there is a successor and it has no locks...
      while (succ != NULL && succ != chunk.tos) {
        if (succ->is_used() && can_move(cur, succ)) {
          latest_can_move = succ;
          break;
        }
        succ = succ->next();
      }

      if (latest_can_move) {
        // auto crc_before = latest_can_move->crc();
        // chunk.dump(latest_can_move, "Moving");
        changes += perform_move(cur, latest_can_move);
        // auto crc_after = cur->crc();
        // if (crc_before != crc_after) {
        //   // chunk.dump(cur, "CRC CHK");
        //   // chunk.dump(cur, "CRC CHECK");
        // }
        // assert(crc_before == crc_after && "Invalid crc after move!");
      }
    }


    if (changes != old_changes) {
      chunk.dump(cur);
      // chunk.dump(cur, "Move");
    }
    cur = cur->next();
  }

  // ssize_t pages_till_wm = (high_watermark - span()) / anchorage::page_size;
  // printf("pages: %zd\n", pages_till_wm);
  // TODO: MADV_DONTNEED those leftover pages if they are beyond a certain point
  return changes;
}

static void longdump(anchorage::Chunk *chunk) {
	for (auto &block : *chunk) {
		block.dump(true);
	}
}



// Run the defragmentation on the set of chunks chosen before
int anchorage::Defragmenter::run(const std::unordered_set<anchorage::Chunk *> &chunks) {
  long start = alaska_timestamp();
  int changes = 0;
  printf("===============[ DEFRAG ]===============\n");
  for (auto *chunk : chunks) {
    printf("before:\n");
    longdump(chunk);
    // chunk->dump(NULL, "Before");
    changes += naive_compact(*chunk);
    printf("after:\n");
    longdump(chunk);
    // chunk->dump(NULL, "After");
  }
  printf("%d changes in %lu\n", changes, alaska_timestamp() - start);
  return changes;
}
