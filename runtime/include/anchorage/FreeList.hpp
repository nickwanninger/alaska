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

  class FirstFitSegFreeList {
   public:
    // This class implements a barebones power-of-two segregated free list with `num_classes`
    // classes. It's not great but hopefully it works
    static constexpr int num_classes = 16;
    static constexpr int num_free_lists = num_classes + 1;

    inline FirstFitSegFreeList(anchorage::Chunk &chunk)
        : m_chunk(chunk) {
      for (int i = 0; i < num_classes; i++) {
        free_lists[i] = LIST_HEAD_INIT(free_lists[i]);
      }
    }

    inline void add(anchorage::Block *blk) {
      size_t size = blk->size();
      auto *list = get_free_list(size);
      auto *node = (struct list_head *)blk->data();
      INIT_LIST_HEAD(node);  // Simply initialize the list
      list_add(node, list);  // Then add it to the free list
      m_size++;              // update the length
    }

    inline void remove(anchorage::Block *blk) {
      // Simply remove the block from the list it's a part of.
      list_del_init((struct list_head *)blk->data());
      m_size--;  // update the length
    }


    inline anchorage::Block *search(size_t size) {
      for (int i = 0; i < num_classes; i++) {
        // TODO: be smarter w/ log or something
        if (size <= (1 << i)) {
          auto blk = search_in_free_list(size, &free_lists[i]);
          if (blk != nullptr) {
            // printf("found %zu req in sc.%d\n", size, i);
            return blk;
          }
        }
      }
      return search_in_free_list(size, &huge_free_list);
    }


    inline void collect(ck::vec<anchorage::Block *> &out) {
      out.ensure_capacity(size());
      for (int i = 0; i < num_classes; i++) {
        collect_from_freelist(&free_lists[i], out);
      }
      collect_from_freelist(&huge_free_list, out);
    }

    inline void collect_freelists(struct list_head *lists[num_free_lists]) {
      for (int i = 0; i < num_classes; i++)
        lists[i] = &free_lists[i];
      lists[num_classes] = &huge_free_list;
    }

    inline size_t size(void) {
      return m_size;
    }

   private:
    inline anchorage::Block *search_in_free_list(size_t size, struct list_head *free_list) {
      struct list_head *cur = nullptr;
      list_for_each(cur, free_list) {
        auto blk = anchorage::Block::get((void *)cur);
        if (blk->size() >= size) {
          remove(blk);
          return blk;
        }
      }
      return nullptr;
    }

    inline void collect_from_freelist(
        struct list_head *free_list, ck::vec<anchorage::Block *> &out) {
      // gather up all the holes
      struct list_head *cur = nullptr;
      list_for_each(cur, free_list) {
        auto blk = anchorage::Block::get((void *)cur);
        out.push(blk);
      }
    }


    struct list_head *get_free_list(size_t object_size) {
      // printf("size %zu\n", object_size);
      for (int i = 0; i < num_classes; i++) {
        // TODO: be smarter w/ log or something
        if (object_size <= (1 << i)) {
          // printf("   class %d\n", i);
          return &free_lists[i];
        }
      }

      // printf("   class 'huge'\n");
      return &huge_free_list;
    }


    // The base class manages access to the chunk if the subclasses need it
    inline auto &chunk() {
      return m_chunk;
    }

    size_t m_size = 0;

    struct list_head free_lists[num_classes];
    struct list_head huge_free_list = LIST_HEAD_INIT(huge_free_list);
    anchorage::Chunk &m_chunk;
  };

}  // namespace anchorage
