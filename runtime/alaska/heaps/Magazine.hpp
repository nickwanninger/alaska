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

#include <alaska/heaps/HeapPage.hpp>
#include <alaska/util/Logger.hpp>
#include <alaska/util/list_head.h>

namespace alaska {

  // A Magazine is just a class which wraps up many HeapPages into a
  // set which can be added and removed from easily.
  // (its a pun)

  class MagazineBase {
   protected:
    struct list_head m_available;
    struct list_head m_recent_full;

    size_t m_count = 0;

   public:
    inline void rebalance(alaska::HeapPage *page) {
      bool was_full = page->was_full;
      if (page->available() == 0 and !was_full) {
        // No space in page. Move to end of full list.
        list_move(&page->mag_list, &this->m_recent_full);
        was_full = true;
      } else if (was_full) {
        // Space in page! Reuse it.
        list_move(&page->mag_list, &this->m_available);
        was_full = false;
      }
    }

    // Place page at the head of m_available (LIFO: most-recently-returned first).
    inline void move_to_available(alaska::HeapPage *page) {
      list_move(&page->mag_list, &this->m_available);
    }

    // Place page at the tail of m_recent_full.
    inline void move_to_full(alaska::HeapPage *page) {
      list_move_tail(&page->mag_list, &this->m_recent_full);
    }
  };

  template <typename T>
  class Magazine final : public MagazineBase {
   public:
    Magazine();
    void add(T *page);
    void remove(T *page);
    T *pop(void);
    void collect();

    inline size_t size(void) const { return m_count; }


    template <typename Fn>
    T *find(Fn f) {
      T *entry, *temp;
      // Search pages we believe have space first.
      list_for_each_entry_safe(entry, temp, &this->m_available, mag_list) {
        if (f(entry)) {
          return entry;
        }
      }

      // Fall back to recently full pages.
      list_for_each_entry_safe(entry, temp, &this->m_recent_full, mag_list) {
        if (f(entry)) {
          return entry;
        }
      }

      return nullptr;
    }


    template <typename Fn>
    void for_each(Fn f) {
      T *entry = nullptr;
      list_for_each_entry(entry, &this->m_available, mag_list) {
        if (!f(entry)) return;
      }
      list_for_each_entry(entry, &this->m_recent_full, mag_list) {
        if (!f(entry)) break;
      }
    }

    // Return the first unowned page from m_available without removing it from the list.
    // Ownership is tracked via set_owner. With the LIFO invariant enforced by put_page,
    // the head is almost always the answer; at most one owned page is skipped.
    T *pop_available(ThreadCache *owner) {
      T *entry = nullptr;
      list_for_each_entry(entry, &this->m_available, mag_list) {
        if (entry->get_owner() == nullptr) return entry;
      }
      return nullptr;
    }

   private:
    static size_t page_avail(T *page) { return page->available(); }
  };

  template <typename T>
  inline Magazine<T>::Magazine() {
    m_available = LIST_HEAD_INIT(m_available);
    m_recent_full = LIST_HEAD_INIT(m_recent_full);
  }


  template <typename T>
  inline void Magazine<T>::add(T *page) {
    list_add_tail(&page->mag_list, &this->m_available);
    page->magazine = this;
    m_count++;
  }



  template <typename T>
  inline void Magazine<T>::remove(T *page) {
    m_count--;

    list_del_init(&page->mag_list);
  }


  template <typename T>
  inline T *Magazine<T>::pop(void) {
    if (list_empty(&this->m_available)) {
      if (!list_empty(&this->m_recent_full)) {
        collect();
      }
    }

    if (list_empty(&this->m_available)) return nullptr;

    m_count--;
    auto hp = list_first_entry(&this->m_available, T, mag_list);
    list_del_init(&hp->mag_list);
    return hp;
  }

  template <typename T>
  inline void Magazine<T>::collect() {
    T *entry, *temp;

    long full = 0;
    long avail = 0;
    // Move pages that have become full out of the available list.
    list_for_each_entry_safe(entry, temp, &this->m_available, mag_list) { rebalance(entry); }

    // Re-promote pages that regained space.
    list_for_each_entry_safe(entry, temp, &this->m_recent_full, mag_list) { rebalance(entry); }
  }

}  // namespace alaska
