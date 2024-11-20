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

#include <alaska/list_head.h>
#include <ck/map.h>
#include <ck/lock.h>
#include <alaska/Logger.hpp>
#include <alaska/alaska.hpp>

namespace alaska {



  // A ThreadRegistry allows threads to be added and removed,
  // and allows a little bit of data to be attached to the thread
  template <typename T>
  class ThreadRegistry final : public alaska::InternalHeapAllocated {
   public:
    ThreadRegistry(void) {}
    using ThreadData = T;

    // Join with the current thread
    void join(ThreadData init_data = {}) {
      log_debug("thread join %lx\n", pthread_self());
      auto l = take_lock();
      m_registry[pthread_self()] = init_data;
    }

    ThreadData leave(void) {
      log_debug("thread leave %lx\n", pthread_self());
      auto l = take_lock();
      auto self = pthread_self();
      auto td = m_registry[self];
      m_registry.remove(self);
      return td;
    }


    // Get the data for the *current thread*
    ThreadData &get_data(void) {
      auto l = take_lock();  // SLOW
      return m_registry[pthread_self];
    }


    template <typename Fn>
    void for_each(Fn f) {
      auto l = take_lock();
      for (auto &[pthread, data] : m_registry)
        f(pthread, data);
    }

    ck::scoped_lock take_lock(void) { return this->m_lock; }


    inline long num_threads() { return m_registry.size(); }

    void lock_thread_creation(void) { m_lock.lock(); }
    void unlock_thread_creation(void) { m_lock.unlock(); }


    template <typename Fn>
    void for_each_locked(Fn f) {
      for (auto &[pthread, data] : m_registry)
        f(pthread, data);
    }

   private:
    ck::mutex m_lock;
    ck::map<pthread_t, ThreadData> m_registry;
  };
}  // namespace alaska
