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

#pragma once
#include <alaska/internal.h>
#include <alaska/list_head.h>

namespace anchorage {

  struct Chunk;  // fwd decl

  /**
   * A Block in the anchorage allocator is the metadata and tracking information for
   * a single allocation in the system.
   */
  struct Block {
    // Strict byte layout here. Must be 16 bytes
    Block *next;              // intrusive list.
    alaska::Mapping *handle;  // if this is null, this block is unallocated


    size_t size(void) const;  // how many bytes is this block?
    void *data(void) const;   // the bytes of this block.
    static Block *get(void *);
    int coalesce_free(Chunk &chunk);

    bool is_free(void) const { return handle == NULL; }
    bool is_used(void) const { return !is_free(); }
  };
  static_assert(sizeof(Block) == 16, "Block is not 16 bytes");




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

    // The main allocator interface for *this* chunk
    Block *alloc(size_t size);
    void free(Block *blk);
    // "Does this chunk contain this allocation"
    bool contains(void *allocation);


    // dump the chunk to stdout for debugging.
    void dump(Block *focus = NULL);

    bool can_move(Block *free_block, Block *to_move);
    int perform_move(Block *free_block, Block *to_move);
    int compact(void);  // perform compaction
    int sweep_freed_but_locked(void);

    class BlockIterator {
      Block *current;

     public:
      using reference = Block &;
      using pointer = Block *;
      inline BlockIterator(Block *current) : current(current) {}
      friend bool operator==(const BlockIterator &a, const BlockIterator &b) { return a.current == b.current; };
      friend bool operator!=(const BlockIterator &a, const BlockIterator &b) { return a.current != b.current; };

      Block &operator*() const { return *current; }
      Block *operator->() { return current; }

      BlockIterator &operator++() {
        current = current->next;
        return *this;
      }

      BlockIterator operator++(int) {
        BlockIterator tmp = *this;
        ++(*this);
        return tmp;
      }
    };

    inline BlockIterator begin(void) { return BlockIterator(front); }
    inline BlockIterator end(void) { return BlockIterator(tos); }
  };

  // main interface to the allocator
  void *alloc(alaska::Mapping &mapping, size_t size);
  void free(alaska::Mapping &mapping, void *ptr);
  void barrier(void);


  void allocator_init(void);
  void allocator_deinit(void);
}  // namespace anchorage
