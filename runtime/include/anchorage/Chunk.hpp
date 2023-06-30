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
#pragma once

#include <anchorage/Anchorage.hpp>
#include <anchorage/Block.hpp>
#include <anchorage/FreeList.hpp>
#include <alaska/list_head.h>
#include <ck/set.h>


namespace anchorage {

  /**
   * A Chunk is allocated on a large mmap region given to us by the kernel.
   * Internally, a chunk is an extremely simple bump allocator. This bump
   * allocator benefits quite significantly from the ability to relocate
   * it's memory (enabled by Alaska, of course), and uses this to great
   * success.
   */
  struct Chunk {
		// FreeList is a friend of chunks.
    friend anchorage::FreeList;

    size_t pages;  // how many 4k pages this chunk uses.
    anchorage::Block
        *front;  // The first block. This is also a pointer to the first byte of the mmap region
    anchorage::Block *tos;      // "Top of stack"
    size_t high_watermark = 0;  // the highest point this chunk has reached.
		
		// How many active bytes are there - tracked by alloc/free
		size_t active_bytes = 0;

		anchorage::FirstFitSingleFreeList free_list;
		// anchorage::FirstFitSegregatedFreeList free_list;
    // struct list_head free_list;

    // ctor/dtor
    Chunk(size_t pages);
    Chunk(Chunk &&) = delete;
    Chunk(const Chunk &) = delete;
    ~Chunk();


    // Given a pointer, get the chunk it is a part of (TODO: make this slightly faster)
    static auto get(void *ptr) -> anchorage::Chunk *;
    // The main allocator interface for *this* chunk
    auto alloc(size_t size) -> anchorage::Block *;
    void free(Block *blk);

    // Coalesce the free block around a previously freed block, `blk`
    auto coalesce_free(Block *blk) -> int;
    // "Does this chunk contain this allocation"
    bool contains(void *allocation);


    // add and remove blocks from the free list
    void fl_add(Block *blk);
    void fl_del(Block *blk);

    // dump the chunk to stdout for debugging.
    void dump(Block *focus = NULL, const char *message = "");
    void dump_free_list(void);

    // get all Chunks
    static auto all(void) -> const ck::set<anchorage::Chunk *> &;

    // the total memory used in this heap
    size_t span(void) const;

    bool split_free_block(anchorage::Block *to_split, size_t required_size);

    inline BlockIterator begin(void) {
      return BlockIterator(front);
    }
    inline BlockIterator end(void) {
      return BlockIterator(tos);
    }


    // Defragment this chunk, returning the saved bytes off the end
    long defragment();

    enum ShiftDirection { Right, Left };

    bool shift_hole(anchorage::Block **hole_ptr, ShiftDirection dir);
    void gather_sorted_holes(ck::vec<anchorage::Block *> &out_holes);
  };

}  // namespace anchorage
