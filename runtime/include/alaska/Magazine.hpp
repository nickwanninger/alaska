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

  template <typename T>
  class Magazine final {
   public:
    Magazine();
    void add(T *page);
    void remove(T *page);
    T *pop(void);

    inline size_t size(void) const { return m_count; }


    template <typename Fn>
    T *find(Fn f) {
      T *entry, *temp;
      // Iterate over the list safely
      list_for_each_entry_safe(entry, temp, &this->list, mag_list) {
        if (f(entry)) {
          return entry;
        }
      }

      return nullptr;
    }


    template <typename Fn>
    void foreach (Fn f) {
      T *entry = nullptr;
      list_for_each_entry(entry, &this->list, mag_list) {
        if (!f(entry)) break;
      }
    }

   private:
    struct list_head list;

    size_t m_count = 0;
  };

  template <typename T>
  inline Magazine<T>::Magazine() {
    list = LIST_HEAD_INIT(list);
  }


  template <typename T>
  inline void Magazine<T>::add(T *page) {
    list_add_tail(&page->mag_list, &this->list);
    m_count++;
  }



  template <typename T>
  inline void Magazine<T>::remove(T *page) {
    m_count--;

    list_del_init(&page->mag_list);
  }


  template <typename T>
  inline T *Magazine<T>::pop(void) {
    if (list_empty(&this->list)) return nullptr;

    m_count--;
    auto hp = list_first_entry(&this->list, T, mag_list);
    list_del_init(&hp->mag_list);
    return hp;
  }


}  // namespace alaska
