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
#include <unordered_set>

namespace anchorage {

  /**
   * A Chunk is allocated on a large mmap region given to us by the kernel.
   * Internally, a chunk is an extremely simple bump allocator. This bump
   * allocator benefits quite significantly from the ability to relocate
   * it's memory (enabled by Alaska, of course), and uses this to great
   * success.
   */
  struct Chunk {
    size_t pages;               // how many 4k pages this chunk uses.
    anchorage::Block *front;    // The first block. This is also a pointer to the first byte of the mmap region
    anchorage::Block *tos;      // "Top of stack"
    size_t high_watermark = 0;  // the highest point this chunk has reached.

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
    // "Does this chunk contain this allocation"
    bool contains(void *allocation);

    // dump the chunk to stdout for debugging.
    void dump(Block *focus = NULL, const char *message = "");

    int sweep_freed_but_locked(void);

    // get all Chunks
    static auto all(void) -> const std::unordered_set<anchorage::Chunk *> &;

    // the total memory used in this heap
    size_t span(void) const;

    inline BlockIterator begin(void) {
      return BlockIterator(front);
    }
    inline BlockIterator end(void) {
      return BlockIterator(tos);
    }
  };

}  // namespace anchorage