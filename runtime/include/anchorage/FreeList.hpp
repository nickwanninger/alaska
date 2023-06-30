#pragma once

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
#include <anchorage/Chunk.hpp>
#include <anchorage/SizeMap.hpp>
#include <alaska/list_head.h>
#include <ck/set.h>

namespace anchorage {

  // A FreeList is the base class for several different kinds of freelist implementations.
  // The main goal is to store free `anchorage::Block`s in a way that
  class FreeList {
   public:
    inline FreeList(anchorage::Chunk &chunk)
        : m_chunk(chunk) {
    }
    virtual ~FreeList(){};

    // Add a block to this free list
    virtual void add(anchorage::Block *) = 0;
    // removve a block from the free list
    virtual void remove(anchorage::Block *) = 0;
    // Search for a block that can fit a certain size. Remove
    // it from the free list and return it.
    virtual auto search(size_t size) -> anchorage::Block * = 0;
    // Collect all the blocks in the free list (No sorting or anything)
    virtual void collect(ck::vec<anchorage::Block *> &out) = 0;
    // How many blocks does the free list constain?
    virtual size_t size(void) = 0;

    // The base class manages access to the chunk if the subclasses need it
    inline auto &chunk() {
      return m_chunk;
    }

   protected:
    anchorage::Chunk &m_chunk;
  };


  class FirstFitSingleFreeList : public FreeList {
   public:
    using FreeList::FreeList;
    inline ~FirstFitSingleFreeList() override {
      // Nothing.
    }
    inline void add(anchorage::Block *blk) override {
      auto *node = (struct list_head *)blk->data();
      INIT_LIST_HEAD(node); // Simply initialize the list
      list_add(node, &free_list); // Then add it to the free list
      m_size++; // update the length
    }

    inline void remove(anchorage::Block *blk) override {
			// Simply remove the block from the list and initialize the list to 0
      list_del_init((struct list_head *)blk->data());
      m_size--; // update the length
    }

    inline anchorage::Block *search(size_t size) override {
      // return nullptr;
      struct list_head *cur = nullptr;
      list_for_each(cur, &free_list) {
        auto blk = anchorage::Block::get((void *)cur);
        if (blk->size() >= size) {
          remove(blk);
          return blk;
        }
      }
      return nullptr;
    }

    inline void collect(ck::vec<anchorage::Block *> &out) override {
      out.ensure_capacity(size());
      // gather up all the holes
      struct list_head *cur = nullptr;
      list_for_each(cur, &free_list) {
        auto blk = anchorage::Block::get((void *)cur);
        out.push(blk);
      }
    }


    inline size_t size(void) override {
      return m_size;
    }

   private:
    size_t m_size = 0;
    // A single first-fit linked list
    struct list_head free_list = LIST_HEAD_INIT(free_list);
  };

}  // namespace anchorage
