/*
 * This file is part of the Alaska Handle-Based Memory Management System
 *
 * Copyright (c) 2024, Nick Wanninger <ncw@u.northwestern.edu>
 * Copyright (c) 2024, The Constellation Project
 * All rights reserved.
 *
 * This is free software.  You are permitted to use, redistribute,
 * and modify it as specified in the file "LICENSE".
 */

#pragma once

#include <alaska/HeapPage.hpp>

namespace alaska {

  // A Magazine is just a class which wraps up many HeapPages into a
  // set which can be added and removed from easily.
  // (its a pun)
  class Magazine final {
   public:
    void add(HeapPage *page);
    void remove(HeapPage *page);
    HeapPage *pop(void);

    inline size_t size(void) const { return m_count; }

   private:
    HeapPage *head = nullptr;
    HeapPage *tail = nullptr;

    size_t m_count = 0;
  };




  inline void Magazine::add(HeapPage *page) {
    m_count++;
    if (head == nullptr) {
      head = page;
      tail = page;
      return;
    }

    tail->m_next = page;
    page->m_prev = tail;
    tail = page;
  }



  inline void Magazine::remove(HeapPage *page) {
    m_count--;
    if (page == head) {
      head = page->m_next;
    }

    if (page == tail) {
      tail = page->m_prev;
    }

    if (page->m_prev != nullptr) {
      page->m_prev->m_next = page->m_next;
    }

    if (page->m_next != nullptr) {
      page->m_next->m_prev = page->m_prev;
    }

    page->m_next = nullptr;
    page->m_prev = nullptr;
  }


  inline HeapPage *Magazine::pop(void) {
    if (head == nullptr) return nullptr;

    m_count--;
    HeapPage *ret = head;
    head = head->m_next;
    if (head != nullptr) {
      head->m_prev = nullptr;
    }
    return ret;
  }


}  // namespace alaska
