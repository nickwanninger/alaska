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

#include <stdlib.h>
#include <anchorage/LinkedList.h>
#include <assert.h>

namespace anchorage {
  /**
   * SubHeap - a region of allocations of a specific size with associated handles
   */
  class SubHeap {
   public:
    SubHeap(off_t page_address, size_t length, uint16_t object_size, uint16_t object_count)
        : m_page(page_address)
        , m_length(length)
        , m_object_size(object_size)
        , m_object_count(object_count) {
      // We only allow (a maximum of) 256 objects per span. This assertion ensures that, but
      // it will never occur as the Arena that allocates these will (hopefully) be sane.
      assert(m_object_count <= 256);

      m_free = m_object_count;
      // zero out the handles array
      for (int i = 0; i < m_object_count; i++)
        m_handles[i] = 0;
    }


    unsigned get_ind(void *object) {
      return ((off_t)object - m_page) / m_object_size;
    }
    void *get_object(unsigned ind) {
      return (void *)(m_page + ind * m_object_size);
    }

    // Allocate an object for a certain alaska mapping, and
    // return a pointer to the object. Will return NULL if
    // there is nothing, for some reason
    void *alloc(alaska::Mapping &m) {
      // First, try to pop off the free list.
      void *obj = m_freelist.try_pop();

      // Then
      if (obj == nullptr) {
        if (m_next_bump < m_object_count) {
          obj = get_object(m_next_bump++);
        }
      }

      if (obj) {
        int ind = get_ind(obj);
        m_handles[ind] = reinterpret_cast<uint64_t>(&m);
        m.ptr = obj;
        m_free--;
        return obj;
      }
      return nullptr;
    }

    void free(void *object) {
      auto object_off = (uint64_t)object;
      if (object_off < m_page || object_off >= m_page + m_length * 4096) return;
      m_free++;
      auto ind = get_ind(object);
      m_freelist.push(object);
      m_handles[ind] = 0;
    }

		auto num_free(void) const { return m_free; }

   protected:
    anchorage::LinkedList m_freelist;
    uint32_t m_handles[256];   // array of handles (indicating which objects are allocated)
    uint64_t m_page;           // The address of the backing memory for this span
    size_t m_length;           // The length of this span in pages
    uint16_t m_next_bump = 0;  // Where the next bump object is
    uint16_t m_free = 0;       // how many objects are free in this span?
    uint16_t m_object_size;    // the size of each object
    uint16_t m_object_count;   // how many objects
  };
}  // namespace anchorage
