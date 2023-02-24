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
#include <alaska/internal.h>
#include <alaska/list_head.h>
#include <unordered_set>

namespace anchorage {

  struct Chunk;  // fwd decl

  /**
   * A Block in the anchorage allocator is the metadata and tracking information for
   * a single allocation in the system.
   */
  struct Block {
   public:
    static auto get(void *) -> Block *;  // get a block given it's data

    auto size(void) const -> size_t;          // how many bytes is this block?
    auto data(void) const -> void *;          // the bytes of this block.
    auto coalesce_free(Chunk &chunk) -> int;  // Perform free-block coalescing

    bool is_free(void) const;  // Is this block free?
    bool is_used(void) const;  // Is this block used?

    // Since we are greatly limited in terms of space in this object, we abstract
    // all accesses to member fields behind functions. This is cause we do some clever
    // nonsense to cram all the data we need into only 16 bytes.
    auto handle(void) const -> alaska::Mapping *;  // get the handle
    void set_handle(alaska::Mapping *handle);      // set the handle
    auto next(void) const -> anchorage::Block *;   // get the next block
    void set_next(anchorage::Block *new_next);     // set the next block (TODO: abstract into LL ops)
    auto prev(void) const -> anchorage::Block *;   // get the prev block
    void set_prev(anchorage::Block *new_prev);     // set the prev block (TODO: abstract into LL ops)

    void dump(bool verbose, bool highlight = false);  // print a block in the fancy Chunk::dump format :)
   private:
    // Strict byte layout here. Must be 16 bytes
    uint32_t m_prev_off;  // how many 16 byte blocks the previous node is
    uint32_t m_next_off;  // how many 16 byte blocks until the next node?
    uint32_t m_handle;    // The handle this block belongs to (basically a 32bit pointer)
    uint32_t m_padding;   // unused... for now
  };

  // Assert that the size of the block is what we expect it to be (16 bytes)
  static_assert(sizeof(anchorage::Block) == 16, "Block is not the right size");



  // building c++ iterators is frustrating, so this abstracts it
  template <typename T>
  class iterator {
   public:
    using reference = T &;
    using pointer = T *;

    friend bool operator==(const iterator &a, const iterator &b) {
      return a.get() == b.get();
    };
    friend bool operator!=(const iterator &a, const iterator &b) {
      return a.get() != b.get();
    };

    auto operator*() const -> reference {
      return *get();
    }
    auto operator->() -> pointer {
      return get();
    }


    auto operator++() -> iterator<T> & {
      step();
      return *this;
    }

    auto operator++(int) -> iterator<T> {
      iterator tmp = *this;
      ++(*this);
      return tmp;
    }

    // You need to implement this.
    virtual void step() = 0;
    virtual T *get() const = 0;
  };

  /**
   * BlockIterator: Given a block, provide c++ iterator semantics
   */
  class BlockIterator : public anchorage::iterator<Block> {
    Block *current;

   public:
    inline BlockIterator(Block *current) : current(current) {
    }

    void step(void) override {
      current = current->next();
    }
    Block *get(void) const override {
      return current;
    }
  };


  /**
   * A Chunk is allocated on a large mmap region given to us by the kernel.
   * Internally, a chunk is an extremely simple bump allocator. This bump
   * allocator benefits quite significantly from the ability to relocate
   * it's memory (enabled by Alaska, of course), and uses this to great
   * success.
   */
  struct Chunk {
    size_t pages;  // how many 4k pages this chunk uses.
    Block *front;  // The first block. This is also a pointer to the first byte of the mmap region
    Block *tos;    // "Top of stack"

    // ctor/dtor
    Chunk(size_t pages);
    Chunk(Chunk &&) = delete;
    Chunk(const Chunk &) = delete;
    ~Chunk();


    // Given a pointer, get the chunk it is a part of (TODO: make this slightly faster)
    static auto get(void *ptr) -> Chunk *;
    // The main allocator interface for *this* chunk
    Block *alloc(size_t size);
    void free(Block *blk);
    // "Does this chunk contain this allocation"
    bool contains(void *allocation);


    // dump the chunk to stdout for debugging.
    void dump(Block *focus = NULL, const char *message = "");

    bool can_move(Block *free_block, Block *to_move);
    int perform_move(Block *free_block, Block *to_move);
    int compact(void);  // perform compaction
    int sweep_freed_but_locked(void);

    inline BlockIterator begin(void) {
      return BlockIterator(front);
    }
    inline BlockIterator end(void) {
      return BlockIterator(tos);
    }
  };


  /**
   * Defragmenter: As the core of the Anchorage service, this class will perform
   * tracking and functionality required to move memory around using various tracking
   * mechanisms and heuristics.
   */
  class Defragmenter {
    // The set of chunks this defragmenter should care about.
    std::unordered_set<anchorage::Chunk *> chunks;

   public:
    void add_chunk(anchorage::Chunk &chunk);
    int run(void);

    // ...
  };

  // main interface to the allocator
  void *alloc(alaska::Mapping &mapping, size_t size);
  void free(alaska::Mapping &mapping, void *ptr);
  void barrier(bool force = false);


  void allocator_init(void);
  void allocator_deinit(void);


  constexpr size_t block_size = sizeof(anchorage::Block);
  constexpr size_t page_size = 4096;
  inline size_t size_with_overhead(size_t sz) {
    return sz + block_size;
  }

  extern size_t moved_bytes;  // How many bytes have been moved by anchorage?
  void *memmove(void *dst, void *src, size_t size);
}  // namespace anchorage
