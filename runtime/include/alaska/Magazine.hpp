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
#include <alaska/Logger.hpp>
#include <alaska/list_head.h>

namespace alaska {

  // A Magazine is just a class which wraps up many HeapPages into a
  // set which can be added and removed from easily.
  // (its a pun)
  class Magazine final {
   public:
    Magazine();
    void add(HeapPage *page);
    void remove(HeapPage *page);
    HeapPage *pop(void);

    inline size_t size(void) const { return m_count; }


    template <typename Fn>
    HeapPage *find(Fn f) {
      HeapPage *entry, *temp;

      // Iterate over the list safely
      list_for_each_entry_safe(entry, temp, &this->list, mag_list) {
        if (f(entry)) {
          // Remove the entry from the list
          // list_del(&entry->mag_list);
          return entry;
        }
      }

      return nullptr;
    }


    template <typename Fn>
    void foreach (Fn f) {
      HeapPage *entry = nullptr;
      list_for_each_entry(entry, &this->list, mag_list) {
        if (!f(entry)) break;
      }
    }

   private:
    struct list_head list;

    size_t m_count = 0;
  };

  inline Magazine::Magazine() { list = LIST_HEAD_INIT(list); }


  inline void Magazine::add(HeapPage *page) {
    list_add_tail(&page->mag_list, &this->list);
    m_count++;
  }



  inline void Magazine::remove(HeapPage *page) {
    m_count--;

    list_del_init(&page->mag_list);
  }


  inline HeapPage *Magazine::pop(void) {
    if (list_empty(&this->list)) return nullptr;

    m_count--;
    auto hp = list_first_entry(&this->list, HeapPage, mag_list);
    list_del_init(&hp->mag_list);
    return hp;
  }


}  // namespace alaska
