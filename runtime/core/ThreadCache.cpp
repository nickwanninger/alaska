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

#include <alaska/ThreadCache.hpp>
#include <alaska/Logger.hpp>



namespace alaska {

  // Stub out the methods of ThreadCache
  void *ThreadCache::halloc(size_t size, bool zero) {

    log_fatal("ThreadCache::halloc not implemented yet!!\n");
    return nullptr;
  }

  void ThreadCache::hfree(void* ptr) {
    log_warn("ThreadCache::hfree(%p) not implemented yet!!\n", ptr);
    return;
  }
}
