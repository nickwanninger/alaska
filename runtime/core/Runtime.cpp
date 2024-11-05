
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


#include <alaska/Runtime.hpp>
#include <alaska/SizeClass.hpp>
#include <alaska/BarrierManager.hpp>
#include <alaska/Localizer.hpp>
#include "alaska/alaska.hpp"
#include "alaska/utils.h"
#include <stdlib.h>
#include <alaska/utils.h>

namespace alaska {
  // The default instance of a barrier manager.
  static BarrierManager global_nop_barrier_manager;
  // The current global instance of the runtime, since we can only have one at a time
  static Runtime *g_runtime = nullptr;
  static volatile bool runtime_initialized = false;


  Runtime::Runtime(alaska::Configuration config)
      : config(config)
      , handle_table(config)
      , heap(config) {
    // Validate that there is not already a runtime (TODO: atomics?)
    ALASKA_ASSERT(g_runtime == nullptr, "Cannot create more than one runtime");

    // Assign the global runtime to be this instance
    atomic_set(g_runtime, this);
    // Attach a default barrier manager
    this->barrier_manager = &global_nop_barrier_manager;

    log_debug("Created a new Alaska Runtime @ %p", this);
    atomic_set(runtime_initialized, true);
  }

  Runtime::~Runtime() {
    log_debug("Destroying Alaska Runtime");
    // Unset the global instance so another runtime can be allocated
    atomic_set(g_runtime, nullptr);
  }


  Runtime &Runtime::get() {
    ALASKA_ASSERT(g_runtime != nullptr, "Runtime not initialized");
    return *g_runtime;
  }
  Runtime *Runtime::get_ptr() {
    return g_runtime;
  }


  void Runtime::dump(FILE *stream) {
    //
    alaska::printf("Alaska Runtime Information:\n");
    heap.dump(stream);
    handle_table.dump(stream);
  }


  void Runtime::dump_html(FILE *stream) {
    fprintf(stream, "<!DOCTYPE html>\n");
    fprintf(stream, "<html>\n");

    fprintf(stream, R"HEAD(
       <head>
         <style>
            :root { --p0-color: #ff00ff; --p1-color: #9C27B0; }
            body { box-sizing: border-box; color: white; background-color: black; text-wrap: nowrap; font-family: monospace; }
            /* td { text-wrap: nowrap; } */
            /* td { line-height: 0; } */
            .el { height: 15px; background-color: #2f2f2f; display: inline-block; /* border-right: 1px solid black; */ }
            .al { background-color: #2fff2f; color: black; }
            .fr { background-color: black !important; }
            .localitydata .al { background-color: white; }
            .p0 { border-bottom: 2px solid var(--p0-color); border-top: 2px solid var(--p0-color); }
            .p1 { border-bottom: 2px solid var(--p1-color); border-top: 2px solid var(--p1-color); }
            .localitydata .p0 { background-color: var(--p0-color); }
            .localitydata .p1 { background-color: var(--p1-color); }
            .pin { background-color: red !important; }


         </style>
       </head>
    )HEAD");
    fprintf(stream, "<body>\n");

    fprintf(stream, "<h1>Heap Pages:</h1>");

    fprintf(stream, "<table>");
    heap.dump_html(stream);
    fprintf(stream, "</table>");


    fprintf(stream, "</body><html>\n");
  }


  ThreadCache *Runtime::new_threadcache(void) {
    auto tc = new ThreadCache(next_thread_cache_id++, *this);
    tcs_lock.lock();
    tcs.add(tc);
    tcs_lock.unlock();
    return tc;
  }

  void Runtime::del_threadcache(ThreadCache *tc) {
    tcs_lock.lock();
    tcs.remove(tc);
    delete tc;
    tcs_lock.unlock();
  }


  void Runtime::lock_all_thread_caches(void) {
    tcs_lock.lock();

    for (auto *tc : tcs)
      tc->lock.lock();
  }
  void Runtime::unlock_all_thread_caches(void) {
    for (auto *tc : tcs)
      tc->lock.unlock();
    tcs_lock.unlock();
  }


  void wait_for_initialization(void) {
    log_debug("waiting for initialization!\n");
    while (not is_initialized()) {
      sched_yield();
    }
    log_debug("Initialized!\n");
  }


  bool is_initialized(void) { return atomic_get(runtime_initialized); }

}  // namespace alaska


// Simply use clock_gettime, which is fast enough on most systems
extern "C" uint64_t alaska_timestamp() {
  struct timespec spec;
  clock_gettime(1, &spec);
  return spec.tv_sec * (1000 * 1000 * 1000) + spec.tv_nsec;
}
