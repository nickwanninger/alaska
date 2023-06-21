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


#include <alaska/alaska.hpp>
#include <assert.h>
#include <anchorage/Anchorage.hpp>
#include <anchorage/Defragmenter.hpp>
#include <anchorage/Swapper.hpp>
#include <anchorage/Chunk.hpp>
#include <anchorage/Block.hpp>
#include <string.h>

inline bool anchorage::Defragmenter::can_move(Block *free_block, Block *to_move) {
  // we are certain to_move has a handle associated, as we would have merged
  // all the free blocks up until to_move

  // First, verify that the handle isn't locked.
  if (to_move->is_locked()) return false;

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

inline int anchorage::Defragmenter::perform_move(
    anchorage::Chunk &chunk, anchorage::Block *free_block, anchorage::Block *to_move) {
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
    // chunk.fl_add(trailing_free);

		chunk.dump(trailing_free, "TF");
    // chunk.coalesce_free(trailing_free);
    // printf("Here\n");
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
      memmove(dst, src, to_move_size);
      // update the handle
      free_block->set_handle(handle);
      handle->ptr = dst;

      // invalidate the handle on the moved block
      to_move->set_handle(nullptr);
      chunk.coalesce_free(to_move);
      return 1;
    } else {
      memmove(dst, src, to_move_size);
      free_block->clear();
      free_block->set_handle(handle);
      handle->ptr = dst;
      to_move->clear();
      to_move->set_handle(nullptr);
      chunk.coalesce_free(to_move);
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
    if (cur->is_free()) {
      // if this block is unallocated, coalesce all `next` free blocks,
      // and move allocated blocks forward if you can.
      chunk.coalesce_free(cur);
      anchorage::Block *succ = cur->next();
      anchorage::Block *latest_can_move = NULL;
#if 0
      // Look over the blocks after cur, and find the span of block that can fit into `cur`, and
      // move them all at once.
      anchorage::Block *cursor, *first, *last;
      cursor = cur->next();
      first = last = nullptr;

      // The total space included in `cur`, including the space occupied by the header.
      uint64_t available_space = cur->size() + anchorage::block_size;
      // The current size of the blocks we plan to move. This must be lte `available_space`
      uint64_t required_space = 0;

      while (cursor != NULL && cursor != chunk.tos) {
        auto cursor_size = cursor->size() + anchorage::block_size;

        if (cursor->is_used() && available_space >= required_space + cursor_size) {
          // if first is null, then we start a new span.
          if (first == NULL) {
            first = cursor;
          }

          if (cursor->is_locked()) {
            first = NULL;
          } else {
            required_space += cursor_size;
          }

        } else {
          // it doesn't fit! We have hit the end of a span we can move.
          last = cursor->prev();
          break;
        }
        last = cursor;
        cursor = cursor->next();
      }

      // we found something to move.
      if (first != NULL) {
        if (last == NULL) last = chunk.tos->prev();
        // printf("found %zu to fit into %zu\n", required_space, available_space);
        // chunk.dump(cur, "slot");
        // chunk.dump(first, "first");
        // chunk.dump(last, "last");
      }
#endif
      chunk.dump(cur, "Defrag");


      // A naive implementation of the two-fingers defrag approach as a test
      // Block *finger = chunk.tos;
      // while (finger != NULL && finger != cur) {
      //   if (finger->is_locked()) {
      //     continue;
      //   }
      //   chunk.dump(finger, "Finger");
      //   if (finger->is_used() && can_move(cur, finger)) {
      //     latest_can_move = succ;
      //     break;
      //   }
      //   finger = finger->prev();
      // }


      // if there is a successor and it has no locks...
      while (succ != NULL && succ != chunk.tos) {
        if (succ->is_used() && can_move(cur, succ)) {
          latest_can_move = succ;
          chunk.dump(succ, "look");
					break;
#ifndef ALASKA_ANCHORAGE_DEFRAG_REORDER
          break;
#endif
        }
        succ = succ->next();
      }


      if (latest_can_move) {
        chunk.dump(latest_can_move, "Move");
        changes += perform_move(chunk, cur, latest_can_move);
        chunk.dump(NULL);
      }

      // printf("\n\n");
    }

    if (changes != old_changes) {
#ifdef ALASKA_ANCHORAGE_PRINT_HEAP
      // chunk.dump(cur, "Defrag");

#endif
    }
    cur = cur->next();
  }

  // chunk.dump(cur, "Done");

  // TODO: MADV_DONTNEED the leftover pages
  return changes;
}


// Run the defragmentation on the set of chunks chosen before
int anchorage::Defragmenter::run(const ck::set<anchorage::Chunk *> &chunks) {
  // return 0;
  int changes = 0;
  for (auto *chunk : chunks) {
    changes += naive_compact(*chunk);
  }
  return changes;
}
