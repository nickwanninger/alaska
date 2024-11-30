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


struct AlaskaThreadState {


  // Track the depth of escape of a given thread. If this number is zero,
  // the thread is in 'managed' code and will eventually poll the barrier
  uint64_t escaped;
  // Why did this thread join? Is it joined?
  int join_status;
#define ALASKA_JOIN_REASON_NOT_JOINED -1        // This thread has not joined the barrier.
#define ALASKA_JOIN_REASON_SIGNAL 0        // This thread was signalled.
#define ALASKA_JOIN_REASON_SAFEPOINT 1     // This thread was at a safepoint
#define ALASKA_JOIN_REASON_ORCHESTRATOR 2  // This thread was the orchestrator
#define ALASKA_JOIN_REASON_ABORT 3  // This thread requires the barrier abort (invalid state, for some reason)
};

namespace alaska {
  namespace barrier {

    // Barrier operational lifetime. It is not recommended to use this interface, and
    // instead use `with_barrier` interface below:
    bool begin();
    void end();

    // Initialization and deinitialization
    void init();
    void deinit();



    void get_pinned_handles(ck::set<void *> &out);

  }  // namespace barrier
}  // namespace alaska
