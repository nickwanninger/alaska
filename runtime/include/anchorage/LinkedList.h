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

#include <stdint.h>
#include <alaska/alaska.hpp>

namespace anchorage {

  inline ALASKA_INLINE void* SLL_Next(void* t) {
    return *(reinterpret_cast<void**>(t));
  }

  inline void ALASKA_INLINE SLL_SetNext(void* t, void* n) {
    *(reinterpret_cast<void**>(t)) = n;
  }

  inline void ALASKA_INLINE SLL_Push(void** list, void* element) {
    SLL_SetNext(element, *list);
    *list = element;
  }


  // A singly linked free list using `void*` values (which are treated as void**)
  class LinkedList {
   public:
    constexpr LinkedList() = default;

    // Not copy constructible or movable.
    LinkedList(const LinkedList&) = delete;
    LinkedList(LinkedList&&) = delete;
    LinkedList& operator=(const LinkedList&) = delete;
    LinkedList& operator=(LinkedList&&) = delete;


    // Try to pop a value off the front of the list. returns NULL if there wasn't anything.
    void* try_pop(void) {
      void* obj = m_list;
      if (unlikely(obj == NULL)) {
        return nullptr;
      }

      void* next = SLL_Next(obj);
      m_list = next;
      m_length--;

      // Make sure the next one is in the cache (if it exists)
      if (likely(next != nullptr)) {
        __builtin_prefetch(next, 0, 0);
      }

      return obj;
    }

    // Push a `void*` to the freelist. This requires that the `void*` is at least `size_t` sized
    void push(void* ptr) {
      SLL_Push(&m_list, ptr);
      m_length++;
    }

    // Is list empty?
    bool empty() const {
      return m_list == nullptr;
    }


    uint32_t length(void) const {
      return m_length;
    }

   private:
    uint32_t m_length = 0;  // current length
    void* m_list = nullptr;
  };



  // A well-typed intrusive doubly linked list.
  template <typename T>
  class TList {
   private:
    class Iter;

   public:
    // The intrusive element supertype.  Use the CRTP to declare your class:
    // class MyListItems : public TList<MyListItems>::Elem { ...
    class Elem {
      friend class Iter;
      friend class TList<T>;
      Elem* next_;
      Elem* prev_;

     protected:
      constexpr Elem()
          : next_(nullptr)
          , prev_(nullptr) {
      }

      // Returns true iff the list is empty after removing this
      bool remove() {
        // Copy out next/prev before doing stores, otherwise compiler assumes
        // potential aliasing and does unnecessary reloads after stores.
        Elem* next = next_;
        Elem* prev = prev_;
        ASSERT(prev->next_ == this);
        prev->next_ = next;
        ASSERT(next->prev_ == this);
        next->prev_ = prev;
        prev_ = nullptr;
        next_ = nullptr;
        return next == prev;
      }

      void prepend(Elem* item) {
        Elem* prev = prev_;
        item->prev_ = prev;
        item->next_ = this;
        prev->next_ = item;
        prev_ = item;
      }

      void append(Elem* item) {
        Elem* next = next_;
        item->next_ = next;
        item->prev_ = this;
        next->prev_ = item;
        next_ = item;
      }
    };

    // Initialize to empty list.
    constexpr TList() {
      head_.next_ = head_.prev_ = &head_;
    }

    // Not copy constructible/movable.
    TList(const TList&) = delete;
    TList(TList&&) = delete;
    TList& operator=(const TList&) = delete;
    TList& operator=(TList&&) = delete;

    bool empty() const {
      return head_.next_ == &head_;
    }

    // Return the length of the linked list. O(n).
    size_t length() const {
      size_t result = 0;
      for (Elem* e = head_.next_; e != &head_; e = e->next_) {
        result++;
      }
      return result;
    }

    // Returns first element in the list. The list must not be empty.
    T* first() const {
      ASSERT(!empty());
      ASSERT(head_.next_ != nullptr);
      return static_cast<T*>(head_.next_);
    }

    // Returns last element in the list. The list must not be empty.
    T* last() const {
      ASSERT(!empty());
      ASSERT(head_.prev_ != nullptr);
      return static_cast<T*>(head_.prev_);
    }

    // Add item to the front of list.
    void prepend(T* item) {
      head_.append(item);
    }

    void append(T* item) {
      head_.prepend(item);
    }

    bool remove(T* item) {
      // must be on the list; we don't check.
      return item->remove();
    }

    // Support for range-based iteration over a list.
    Iter begin() const {
      return Iter(head_.next_);
    }
    Iter end() const {
      return Iter(const_cast<Elem*>(&head_));
    }

    // Iterator pointing to a given list item.
    // REQUIRES: item is a member of the list.
    Iter at(T* item) const {
      return Iter(item);
    }

   private:
    // Support for range-based iteration over a list.
    class Iter {
      friend class TList;
      Elem* elem_;
      explicit Iter(Elem* elem)
          : elem_(elem) {
      }

     public:
      Iter& operator++() {
        elem_ = elem_->next_;
        return *this;
      }
      Iter& operator--() {
        elem_ = elem_->prev_;
        return *this;
      }

      bool operator!=(Iter other) const {
        return elem_ != other.elem_;
      }
      bool operator==(Iter other) const {
        return elem_ == other.elem_;
      }
      T* operator*() const {
        return static_cast<T*>(elem_);
      }
      T* operator->() const {
        return static_cast<T*>(elem_);
      }
    };
    friend class Iter;

    Elem head_;
  };
}  // namespace anchorage
