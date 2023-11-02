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
#include <pthread.h>

namespace alaska {
  namespace barrier {

    // Thread tracking lifetime
    void add_self_thread(void);
    void remove_self_thread(void);

    // Barrier operational lifetime.
    void begin();
    void end();

    // Initialization and deinitialization
    void init();
    void deinit();

		void initialize_safepoint_page();
		void block_access_to_safepoint_page();
		void allow_access_to_safepoint_page();

    void get_locked(ck::set<void*> &out);

  }  // namespace barrier
}  // namespace alaska
