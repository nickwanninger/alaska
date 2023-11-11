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
