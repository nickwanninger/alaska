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

#include <alaska/alaska.hpp>
#include <alaska/sim/HTLB.hpp>

namespace alaska::sim {

  // This class provides a way to use handles in the runtime without a
  // compiler transformation. It's mostly used for testing.  It is
  // *not* meant for production, and if a global HTLB simulator is
  // allocated for this thread, every translation will notify that for
  // testing the hardware extension
  template <typename T>
  class handle_ptr final {
   public:
    handle_ptr(void)
        : m_handle(nullptr) {}
    handle_ptr(nullptr_t n)
        : m_handle(n) {}
    handle_ptr(T* raw)
        : m_handle(raw) {}
    handle_ptr(handle_ptr<T>&& h)
        : m_handle(h.m_handle) {}
    handle_ptr(const handle_ptr<T>& h)
        : m_handle(h.m_handle) {}



    handle_ptr<T> operator=(T* h) {
      m_handle = h;
      return *this;
    }

    handle_ptr<T> operator=(const handle_ptr<T>& h) {
      m_handle = h.m_handle;
      return *this;
    }

    // overloaded operators
    T* operator->() const noexcept { return translate(); }
    T& operator*() const noexcept { return *translate(); }


    bool operator==(const handle_ptr<T>& h) const { return m_handle == h.m_handle; }

    operator T*(void) const { return m_handle; }
    T* get(void) const { return m_handle; }


    T* translate(void) const {
      auto m = alaska::Mapping::from_handle_safe(m_handle);
      if (m == nullptr) return m_handle;
      if (alaska::sim::HTLB::get() != nullptr) {
        alaska::sim::HTLB::get()->access(*m, 0);
      }
      return (T*)m->get_pointer();
    }

   private:
    T* m_handle = nullptr;
  };


  template <typename T, typename... Args>
  alaska::sim::handle_ptr<T> alloc(Args&&... args) {
    if (alaska::sim::HTLB::get() == nullptr) {
      abort();
    }
    alaska::sim::handle_ptr<T> ptr =
        (T*)alaska::sim::HTLB::get()->thread_cache->halloc(sizeof(T), true);

    new (&*ptr) T(std::forward<Args>(args)...);

    return ptr;
  }



  template <typename T>
  void release(alaska::sim::handle_ptr<T> h) {
    if (alaska::sim::HTLB::get() == nullptr) {
      abort();
    }
    alaska::sim::HTLB::get()->thread_cache->hfree(h);
  }
}  // namespace alaska::sim


namespace std {
  template <typename T>
  struct hash<alaska::sim::handle_ptr<T>> {
    std::size_t operator()(const alaska::sim::handle_ptr<T>& mc) const {
      // its a pointer. just hash the pointer (return it)
      return (size_t)mc.get();
    }
  };
}  // namespace std
