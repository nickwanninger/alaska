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
#include "alaska/alaska.hpp"
#include <stdlib.h>

namespace alaska {


  struct NopBarrierManager final : public alaska::BarrierManager {
    ~NopBarrierManager() override = default;
    void begin(void) override {}
    void end(void) override {}
  };

  static NopBarrierManager global_nop_barrier_manager;

  static Runtime *g_runtime = nullptr;


  Runtime::Runtime() {
    ALASKA_ASSERT(g_runtime == nullptr, "Cannot create more than one runtime");
    g_runtime = this;
    // Attach a default barrier manager
    this->barrier_manager = &global_nop_barrier_manager;

    log_debug("Created a new Alaska Runtime @ %p", this);
  }

  Runtime::~Runtime() {
    log_debug("Destroying Alaska Runtime");
    g_runtime = nullptr;
  }


  Runtime &Runtime::get() {
    ALASKA_ASSERT(g_runtime != nullptr, "Runtime not initialized");
    return *g_runtime;
  }


  void Runtime::dump(FILE *stream) {
    //
    fprintf(stream, "Alaska Runtime Information:\n");
    heap.dump(stream);
    handle_table.dump(stream);
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


}  // namespace alaska
