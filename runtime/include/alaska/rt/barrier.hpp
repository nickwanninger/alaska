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


#include <alaska/alaska.hpp>
#include <ck/set.h>
#include <ck/func.h>
#include <pthread.h>


typedef struct {
  // Track the depth of escape of a given thread. If this number is zero,
  // the thread is in 'managed' code and will eventually poll the barrier
  uint64_t escaped;
  // Why did this thread join? Is it joined?
  int join_status;
#define ALASKA_JOIN_REASON_NOT_JOINED -1        // This thread has not joined the barrier.
#define ALASKA_JOIN_REASON_SIGNAL 0        // This thread was signalled.
#define ALASKA_JOIN_REASON_SAFEPOINT 1     // This thread was at a safepoint
#define ALASKA_JOIN_REASON_ORCHESTRATOR 2  // THis thread was the orchestrator

  // ...
} alaska_thread_state_t;

extern __thread alaska_thread_state_t alaska_thread_state;


namespace alaska {
  namespace barrier {

    // Thread tracking lifetime
    void add_self_thread(void);
    void remove_self_thread(void);

    // Barrier operational lifetime. It is not recommended to use this interface, and
    // instead use `with_barrier` interface below:
    void begin();
    void end();

    // struct BarrierInfo {};
    // bool with_barrier(ck::func<void(BarrierInfo &)> &&cb);

    // Initialization and deinitialization
    void init();
    void deinit();



    void get_locked(ck::set<void *> &out);

  }  // namespace barrier
}  // namespace alaska
