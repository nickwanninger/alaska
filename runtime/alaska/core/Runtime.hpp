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
#include <alaska/handles/HandleTable.hpp>
#include <alaska/work/BarrierManager.hpp>
#include <alaska/util/Logger.hpp>
#include <alaska/core/ThreadCache.hpp>
#include <alaska/work/WorkScheduler.hpp>
#include <alaska/heaps/Heap.hpp>
#include <alaska/heaps/HugeAllocator.hpp>
#include <alaska/alaska.hpp>
#include <ck/set.h>
#include <alaska/Configuration.hpp>
#include <alaska/core/Runtime.hpp>
#include <alaska/util/RateCounter.hpp>
#include <alaska/work/BarrierWorker.hpp>


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
  struct Runtime final : public alaska::PersistentAllocation {
    alaska::Configuration config;
    // The handle table is a global table that maps handles to their corresponding memory blocks.
    alaska::HandleTable handle_table;

    // This is the actual heap
    alaska::Heap heap;

    // Huge object allocator for objects >= max_large_size (bypasses handle table)
    alaska::HugeAllocator huge_allocator;

    // This is a set of all the active thread caches in the system
    ck::set<alaska::ThreadCache *> tcs;
    ck::mutex tcs_lock;

    // A pointer to the runtime's current barrier manager.
    // This is defaulted to a "nop" manager which simply does nothing.
    alaska::BarrierManager *barrier_manager;
    alaska::RateCounter stat_barriers;
    struct list_head barrier_workers = LIST_HEAD_INIT(barrier_workers);

    // The work scheduler
    alaska::WorkScheduler scheduler;

    // Return the singleton instance of the Runtime if it has been allocated. Abort otherwise.
    static Runtime &get();
    static Runtime *get_ptr();

    Runtime(alaska::Configuration config = {});
    ~Runtime();

    // Allocate and free thread caches.
    ThreadCache *new_threadcache(void);
    void del_threadcache(ThreadCache *);

    // Check if a pointer is a valid handle
    bool is_valid_handle(void *p);
    RateCounter handle_faults;
    // Deal with a handle fault on some handle pointer `handle`
    int handle_fault(uint64_t handle);


    long barrier_work_count(void) {
      long work_count = 0;
      struct list_head *pos;
      list_for_each(pos, &barrier_workers) { work_count++; }
      return work_count;
    }

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
        stat_barriers++;
        barrier_manager->barrier_count++;
        cb();

        // now that the barrier's main function has completed, we need to run the
        // barrier workers to finish the barrier. We need to do this with a 'safe'
        // loop because the barrier workers can remove themselves from the list

        long work_count = 0;
        struct list_head *pos, *n;
        list_for_each_safe(pos, n, &barrier_workers) {
          work_count++;
          auto worker = list_entry(pos, alaska::BarrierWorker, m_list);
          auto res = worker->barrier_work();
          if (res == BarrierWorkResult::BARRIER_WORK_DONE) list_del(pos);
        }

        in_barrier = false;
        barrier_manager->end();
      } else {
        alaska::printf("Barrier failed\n");
        barrier_manager->end();
      }
      unlock_all_thread_caches();
      return true;
    }


    template <typename Fn>
    void walk_handles(alaska::Mapping *m, Fn &&cb) {
      // assume m is valid, call `cb` for each handle we see in the data of m.
      void **data = (void **)m->get_pointer();
      auto *header = alaska::ObjectHeader::from(data);
      size_t count = header->object_size() / sizeof(void *);
      for (size_t i = 0; i < count; i++) {
        void *p = data[i];
        if (p == nullptr) continue;

        auto *mp = alaska::Mapping::from_handle_safe(p);
        if (mp != nullptr) {
          if (handle_table.valid_handle(mp)) {
            cb(mp);
          }
        }
      }
    }


    struct HeapReport {
      size_t committed_bytes = 0;  // How many bytes are committed?
      size_t total_handles = 0;    // The number of objects in the heap (handle table).
      size_t object_bytes = 0;     // How many bytes are used by objects?


      // These stats count pointers between objects in the heap.
      // The main thing I am interested in is how many pointers point out of the page vs. in.
      size_t out_pointers = 0;
      size_t in_pointers = 0;
      // This is similar, but counts the handle table entries instead of
      // pointers (measures the locality of the handle table, which we *can not*
      // improve)
      size_t out_handles = 0;
      size_t in_handles = 0;

      // How many objects are localized
      size_t total_localized = 0;
    };
    HeapReport grade_heap(void);


   private:
    bool in_barrier = false;
    int next_thread_cache_id = 0;
    unsigned long last_barrier_time = 0;
    unsigned long min_barrier_interval = 0;  //  10 * 1000 * 1000;


    void lock_all_thread_caches(void);
    void unlock_all_thread_caches(void);
  };




  // Spin until the runtime has been initialized somehow
  void wait_for_initialization(void);
  // Has the runtime been initialized?
  bool is_initialized(void);


  // called from translate.cpp This function takes a handle (as a
  // pointer that was paassed to alaska_translate) and performs the
  // required operations to handle the fault and return a translated
  // pointer. This is considered a *very* slow path operation, and
  // conditional calls to this function are placed in every alaska
  // translate site, so it also returns the result of translating the
  // handle to avoid extra control flow at the call sites.  it is
  // expected that, at least to some extent, this function will return
  // promptly and not block (for too long). It is *not* called in an
  // environment where alaska barriers can be performed as an
  // optimization.
  __attribute__((preserve_all, pure)) void *do_handle_fault_and_translate(uint64_t handle) asm(
      "alaska.HF");

  struct LocalityReport {
    size_t out_pointers = 0;  // How many pointers point out of the block?
    size_t in_pointers = 0;   // How many pointers point into the same block?
    size_t object_bytes = 0;  // How many bytes are used by objects?
    void dump();
    float locality() {
      size_t total = out_pointers + in_pointers;
      if (total == 0) return 1.0f;
      return (float)in_pointers / (float)total;
    }
  };

  // Grade the locality of an object graph starting at `root`. Not sure how to deal with cycles yet.
  LocalityReport grade_locality(alaska::Mapping &root, int max_depth = 15);
  void grade_locality(alaska::Mapping &root, int max_depth, LocalityReport &report);
}  // namespace alaska
