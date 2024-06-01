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


#include <sys/mman.h>
#include <alaska/Logger.hpp>
#include <alaska/Heap.hpp>
#include "alaska/utils.h"

namespace alaska {


  PageManager::PageManager(void) {
    auto prot = PROT_READ | PROT_WRITE;
    auto flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE;
    this->heap = mmap(NULL, alaska::heap_size, prot, flags, -1, 0);
    ALASKA_ASSERT(
        this->heap != MAP_FAILED, "Failed to allocate the heap's backing memory. Aborting.");

    log_debug("PageManager: Heap allocated at %p", this->heap);
  }

  PageManager::~PageManager() {
    log_debug("PageManager: Deallocating heap at %p", this->heap);
    munmap(this->heap, alaska::heap_size);
  }


  HeapPage *PageManager::get_page(size_t index) {
    log_warn("PageManager: get_page not implemented");
    return NULL;
  }

}  // namespace alaska