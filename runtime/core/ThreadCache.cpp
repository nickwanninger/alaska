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
#include <alaska/Runtime.hpp>
#include <alaska/SizeClass.hpp>
#include "alaska/utils.h"



namespace alaska {



  ThreadCache::ThreadCache(alaska::Runtime &rt)
      : runtime(rt) {
    handle_slab = runtime.handle_table.new_slab();
  }



  SizedPage *ThreadCache::new_sized_page(int cls) {
    // Get a new heap
    auto *heap = runtime.heap.get(alaska::class_to_size(cls));

    // And set the owner
    heap->set_owner(this);
    log_info("ThreadCache::halloc got new heap: %p. Avail = %lu", heap, heap->available());

    // Swap the heaps in the thread cache
    // if (size_classes[cls] != nullptr) runtime.heap.put(size_classes[cls]);
    size_classes[cls] = heap;

    ALASKA_ASSERT(heap->available() > 0, "New heap must have space");
    log_info("new heaps avail = %lu", heap->available());
    return heap;
  }

  // Stub out the methods of ThreadCache
  void *ThreadCache::halloc(size_t size, bool zero) {
    log_info("ThreadCache::halloc size=%zu", size);
    // Allocate a new mapping
    Mapping *m = new_mapping();
    log_info("ThreadCache::halloc mapping=%p", m);

    int cls = alaska::size_to_class(size);

    auto *page = size_classes[cls];
    if (unlikely(page == nullptr)) page = new_sized_page(cls);


    void *ptr = page->alloc(*m, size);
    if (unlikely(ptr == nullptr)) {
      // OOM?
      page = new_sized_page(cls);
      ptr = page->alloc(*m, size);
      ALASKA_ASSERT(ptr != nullptr, "OOM!");
    }


    log_info("ptr = %p", ptr);
    m->set_pointer(ptr);

    void *out = m->to_handle();
    log_info("handle = %p", out);
    return out;
  }


  void *ThreadCache::hrealloc(void *handle, size_t new_size) {
    log_fatal("ThreadCache::hrealloc not implemented yet!!\n");
    return nullptr;
  }


  void ThreadCache::hfree(void *handle) {
    alaska::Mapping *m = alaska::Mapping::from_handle(handle);
    void *ptr = m->get_pointer();

    // Grab the page from the global heap (walk the page table).
    auto *page = this->runtime.heap.pt.get_unaligned(ptr);
    ALASKA_ASSERT(page != NULL, "calling hfree should always return a heap page");
    printf("page = %p\n", page);

    if (page->is_owned_by(this)) {
      log_trace("Free handle %p locally", handle);
      page->release_local(*m, ptr);
    } else {
      log_trace("Free handle %p remotely", handle);
      page->release_remote(*m, ptr);
    }
    // Return the handle to the handle table.
    this->runtime.handle_table.put(m);
  }

  Mapping *ThreadCache::new_mapping(void) {
    auto m = handle_slab->get();

    if (unlikely(m == NULL)) {
      handle_slab = runtime.handle_table.new_slab();
      // This BETTER work!
      m = handle_slab->get();
    }

    return m;
  }

}  // namespace alaska
