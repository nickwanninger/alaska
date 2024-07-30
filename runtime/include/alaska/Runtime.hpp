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
#include <alaska/HandleTable.hpp>
#include <alaska/BarrierManager.hpp>
#include <alaska/Logger.hpp>
#include <alaska/ThreadCache.hpp>
#include <alaska/Heap.hpp>
#include <alaska/alaska.hpp>
#include <ck/set.h>

namespace alaska {
  /**
   * @brief The Runtime class is a container for the global state of the Alaska runtime.
   *
   * It is encapsulated in this class to allow for easy testing of the runtime. The application
   * links against a separate library which provides an interface to a singleton instance of this
   * through a C API (see `alaska.h`, ex: halloc()).
   *
   * This class' main job is to tie together all the various components of the runtime. These
   * components should operate (mostly) independently of each other, and this class should be the
   * only place where they interact.
   *
   * Components:
   *  - HandleTable: A global table that maps handles to their corresponding memory blocks.
   *  - Heap: A global memory manager that allocates and frees memory blocks.
   *  - ThreadCaches: a list of the thread caches currently alive in the system.
   */
  struct Runtime final : public alaska::InternalHeapAllocated {
    // The handle table is a global table that maps handles to their corresponding memory blocks.
    alaska::HandleTable handle_table;

    // This is the actual heap
    alaska::Heap heap;


    // This is a set of all the active thread caches in the system
    ck::set<alaska::ThreadCache *> tcs;
    ck::mutex tcs_lock;

    // A pointer to the runtime's current barrier manager.
    // This is defaulted to a "nop" manager which simply does nothing.
    alaska::BarrierManager *barrier_manager;


    // Return the singleton instance of the Runtime if it has been allocated. Abort otherwise.
    static Runtime &get();


    Runtime();
    ~Runtime();

    // Allocate and free thread caches.
    ThreadCache *new_threadcache(void);
    void del_threadcache(ThreadCache *);
    void dump(FILE *stream);

   private:
    int next_thread_cache_id = 0;
  };
}  // namespace alaska
