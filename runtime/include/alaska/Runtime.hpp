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
#include <alaska/Configuration.hpp>
#include <alaska/Localizer.hpp>

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
    alaska::Configuration config;
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
    static Runtime *get_ptr();


    // Localization logic is broken down into "epochs", and objects
    // can only be re-localized after a certain number of epochs.
    // These are of an arbitrary unit.
    uint64_t localization_epoch = 0;

    bool in_barrier = false;


    Runtime(alaska::Configuration config = {});
    ~Runtime();

    // Allocate and free thread caches.
    ThreadCache *new_threadcache(void);
    void del_threadcache(ThreadCache *);
    void dump(FILE *stream);


    void dump_html(FILE *stream);

    uint64_t handle_faults = 0;
    int handle_fault(uint64_t handle);


    template <typename Fn>
    bool with_barrier(Fn &&cb) {
      auto now = alaska_timestamp();
      if (now - last_barrier_time < min_barrier_interval) {
        return false;
      }
      last_barrier_time = now;

      lock_all_thread_caches();
      if (barrier_manager->begin()) {
        in_barrier = true;
        barrier_manager->barrier_count++;
        cb();
        in_barrier = false;
        barrier_manager->end();
      } else {
        alaska::printf("Barrier failed\n");
        barrier_manager->end();
      }
      unlock_all_thread_caches();
      return true;
    }

   private:
    int next_thread_cache_id = 0;


    unsigned long last_barrier_time = 0;
    unsigned long min_barrier_interval = 10 * 1000 * 1000;


    void lock_all_thread_caches(void);
    void unlock_all_thread_caches(void);
  };


  // Spin until the runtime has been initialized somehow
  void wait_for_initialization(void);
  // Has the runtime been initialized?
  bool is_initialized(void);


  // called from translate.cpp
  int do_handle_fault(uint64_t handle);
}  // namespace alaska
