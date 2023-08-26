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

#include <pthread.h>

namespace alaska {
  namespace barrier {

    void add_thread(pthread_t *);
    void remove_thread(pthread_t *);

    // Barrier operational lifetime.
    void begin();
    void end();

    // Initialization and deinitialization
    void init();
    void deinit();

		void initialize_safepoint_page();

  }  // namespace barrier
}  // namespace alaska
