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
#include <alaska/core/ThreadCache.hpp>

namespace alaska::sim {

  // Thread-local thread cache used by alloc() and release().
  // Call set_thread_cache() in test SetUp / TearDown.
  inline thread_local alaska::ThreadCache* g_thread_cache = nullptr;

  inline void set_thread_cache(alaska::ThreadCache* tc) { g_thread_cache = tc; }

  // A smart-pointer wrapper around an alaska handle, used in tests and
  // tooling to perform handle-based allocation without a compiler
  // transformation.  Not intended for production use.
  template <typename T>
  class handle_ptr final {
   public:
    handle_ptr(void)
        : m_handle(nullptr) {}
    handle_ptr(std::nullptr_t n)
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

    T* operator->() const noexcept { return translate(); }
    T& operator*() const noexcept { return *translate(); }

    bool operator==(const handle_ptr<T>& h) const { return m_handle == h.m_handle; }

    operator T*(void) const { return m_handle; }
    T* get(void) const { return m_handle; }

    T* translate(void) const {
      auto m = alaska::Mapping::from_handle_safe(m_handle);
      if (m == nullptr) return m_handle;
      return (T*)m->get_pointer();
    }

    T* translate_untracked(void) const { return translate(); }

   private:
    T* m_handle = nullptr;
  };


  template <typename T, typename... Args>
  handle_ptr<T> alloc(Args&&... args) {
    if (g_thread_cache == nullptr) abort();
    handle_ptr<T> ptr = (T*)g_thread_cache->halloc(sizeof(T));
    new (&*ptr) T(std::forward<Args>(args)...);
    return ptr;
  }

  template <typename T>
  void release(handle_ptr<T> h) {
    if (g_thread_cache == nullptr) abort();
    g_thread_cache->hfree(h);
  }

}  // namespace alaska::sim


namespace std {
  template <typename T>
  struct hash<alaska::sim::handle_ptr<T>> {
    std::size_t operator()(const alaska::sim::handle_ptr<T>& mc) const {
      return (size_t)mc.get();
    }
  };
}  // namespace std
