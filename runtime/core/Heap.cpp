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

    // Set the bump allocator to the start of the heap.
    this->bump = this->heap;
    this->end = (void *)((uintptr_t)this->heap + alaska::heap_size);

    // Initialize the free list w/ null, so the first allocation is a simple bump.
    this->free_list = nullptr;

    log_debug("PageManager: Heap allocated at %p", this->heap);
  }

  PageManager::~PageManager() {
    log_debug("PageManager: Deallocating heap at %p", this->heap);
    munmap(this->heap, alaska::heap_size);
  }


  void *PageManager::alloc_page(void) {
    ck::scoped_lock lk(this->lock);  // TODO: don't lock.



    if (this->free_list != nullptr) {
      // If we have a free page, pop it off the free list and return it.
      FreePage *fp = this->free_list;
      this->free_list = fp->next;
      log_trace("PageManager: reusing free page at %p", fp);
      return (void *)fp;
    }

    // If we don't have a free page, we need to allocate a new one with the bump allocator.
    void *page = this->bump;
    log_trace("PageManager: bumping to %p", this->bump);
    this->bump = (void *)((uintptr_t)this->bump + alaska::page_size);

    log_trace("PageManager: end = %p", this->end);

    // TODO: this is *so unlikely* to happen. This check is likely expensive and not needed.
    ALASKA_ASSERT(page < this->end, "Out of memory in the page manager.");

    return page;
  }


  void PageManager::free_page(void *page) {
    // TODO: we do not validate the page here for performance reasons.
    ck::scoped_lock lk(this->lock);  // TODO: don't lock.

    // cast the page to a FreePage to store metadata in.
    FreePage *fp = (FreePage *)page;

    // Super simple: push to the free list.
    fp->next = this->free_list;
    this->free_list = fp;
  }

}  // namespace alaska