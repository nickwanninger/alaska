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

#include <alaska/lib.hpp>
#include <anchorage/Anchorage.hpp>

namespace anchorage {

  /**
   * A Block in the anchorage allocator is the metadata and tracking information for
   * a single allocation in the system.
   */
  struct Block {
   public:
    // Initialize the block with default state
    void clear(void);
    // Get a block, given a pointer to it's data
    static auto get(void *) -> Block *;

    auto size(void) const -> size_t;  // how many bytes is this block?
    auto data(void) const -> void *;  // the bytes of this block.
    // Perform free-block coalescing. That is, merge free blocks to the left and right
    // of this block into a single block.
    auto coalesce_free(Chunk &chunk) -> int;

    bool is_free(void) const;  // Is this block free?
    bool is_used(void) const;  // Is this block used?

    // Since we are greatly limited in terms of space in this object, we abstract
    // all accesses to member fields behind functions. This is cause we do some clever
    // nonsense to cram all the data we need into only 16 bytes.
    auto handle(void) const -> alaska::Mapping *;  // get the handle
    void set_handle(alaska::Mapping *handle);      // set the handle
    void clear_handle(void);                       // clear the handle
    auto next(void) const -> anchorage::Block *;   // get the next block
    void set_next(anchorage::Block *new_next);     // set the next block (TODO: abstract into LL ops)
    auto prev(void) const -> anchorage::Block *;   // get the prev block
    void set_prev(anchorage::Block *new_prev);     // set the prev block (TODO: abstract into LL ops)


    auto crc(void) -> uint32_t;                       // compute the crc32 of the data in the block
    void dump(bool verbose, bool highlight = false);  // print a block in the fancy Chunk::dump format :)


    void mark_locked(bool);  // Mark the locked status of this allocation (cannot move)
    bool is_locked(void);    // Can this block move? (locked=no, unlocked=yes)

   private:
    // Strict byte layout here. Must be 16 bytes
    uint32_t m_prev_off;  // how many 16 byte blocks the previous node is
    uint32_t m_next_off;  // how many 16 byte blocks until the next node?
    uint32_t m_handle;    // The handle this block belongs to (basically a 32bit pointer)

    union {
      uint32_t m_flags;
      struct {
        bool m_locked : 1;
      };
    };
  };


  // Assert that the size of the block is what we expect it to be (16 bytes)
  static_assert(sizeof(anchorage::Block) == anchorage::block_size, "Block is not the right size");


  /**
   * BlockIterator: Given a block, provide c++ iterator semantics
   */
  class BlockIterator : public alaska::iterator<anchorage::Block> {
    anchorage::Block *current;

   public:
    inline BlockIterator(anchorage::Block *current)
        : current(current) {
    }

    void step(void) override {
      current = current->next();
    }
    anchorage::Block *get(void) const override {
      return current;
    }
  };


}  // namespace anchorage
