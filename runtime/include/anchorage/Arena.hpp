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

#include <anchorage/LinkedList.h>
#include <anchorage/Anchorage.hpp>
#include <anchorage/SubHeap.hpp>
#include <anchorage/Bitmap.hpp>
#include <anchorage/Arena.hpp>
#include "heaps/top/mmapheap.h"

#include <stdlib.h>
#include <assert.h>

// Pull in heaplayers for the main heap's backing data source
#include <heaplayers.h>

#include <ck/vec.h>


namespace anchorage {


  // All an arena does is allow you to allocate and free pages
  class Arena : public HL::SizedMmapHeap {
    typedef HL::SizedMmapHeap SuperHeap;

   public:
    enum { Alignment = anchorage::page_size };

    explicit Arena();
    // ~Arena();
    inline bool contains(const void *ptr) const {
      auto arena = (uintptr_t)(m_start);
      auto ptrval = (uintptr_t)(ptr);
      return arena <= ptrval && ptrval < arena + m_length;
    }

    void *palloc(Extent &result, size_t pages);
    void pfree(const Extent &extent);


   private:
    void *m_start;
    size_t m_length;  // in bytes

    // A bitmap of all the pages we are tracking. 0 = free, 1 = allocated
    anchorage::Bitmap m_bits;


    // Get either the default heap size, or parse the environment variable if it was passed.
    static size_t get_heap_size(void) {
      size_t size = anchorage::defaultArenaSize;
      if (char *param = getenv("ANCHORAGE_ARENA_SIZE_MB")) {
        size = (uint64_t)atoi(param) * MB;
      }
      return size;
    }
  };


  inline anchorage::Arena::Arena()
      : m_length(get_heap_size())
      , m_bits(m_length / anchorage::page_size) {
    m_bits.clear_all();

    printf("Anchorage arena length = 0x%lx\n", m_length);
    m_start =
        (void *)mmap(NULL, m_length, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (m_start == MAP_FAILED) {
      fprintf(stderr, "Anchorage: failed to mmap backing memory\n");
      exit(EXIT_FAILURE);
    }
    printf("Anchorage arena start  = %p\n", m_start);
  }


  inline void *anchorage::Arena::palloc(Extent &result, size_t pages) {
    // TODO: this can be *way faster*. Just getting it correct before optimizing.
    for (uint64_t i = 0; i < m_bits.size() - pages; i++) {
      bool valid = true;
      for (size_t page = 0; page < pages; page++) {
        if (m_bits.get(i + page)) {
          valid = false;
          break;
        }
      }
      if (valid) {
        // set all the bits to the allocated state
        for (size_t page = 0; page < pages; page++)
          m_bits.set(i + page);
        result.offset = i;
        result.length = pages;
        return (void *)((off_t)m_start + i * anchorage::page_size);
      }
    }
    return nullptr;
  }

  inline void anchorage::Arena::pfree(const Extent &result) {
    // set all the bits to the allocated state
    for (size_t page = 0; page < result.length; page++)
      m_bits.clear(result.offset + page);
  }


}  // namespace anchorage
