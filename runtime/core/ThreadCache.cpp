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
#include <alaska/alaska.hpp>
#include <alaska/utils.h>



namespace alaska {



  ThreadCache::ThreadCache(int id, alaska::Runtime &rt)
      : id(id)
      , runtime(rt) {
    handle_slab = runtime.handle_table.new_slab(this);
  }


  void *ThreadCache::allocate_backing_data(const alaska::Mapping &m, size_t size) {
    int cls = alaska::size_to_class(size);
    auto *page = size_classes[cls];
    if (unlikely(page == nullptr)) page = new_sized_page(cls);
    void *ptr = page->alloc(m, size);
    if (unlikely(ptr == nullptr)) {
      // OOM?
      page = new_sized_page(cls);
      ptr = page->alloc(m, size);
      ALASKA_ASSERT(ptr != nullptr, "OOM!");
    }
    return ptr;
  }


  SizedPage *ThreadCache::new_sized_page(int cls) {
    // Get a new heap
    auto *heap = runtime.heap.get_sizedpage(alaska::class_to_size(cls), this);

    // And set the owner
    heap->set_owner(this);
    log_info("ThreadCache::halloc got new heap: %p. Avail = %lu", heap, heap->available());

    // Swap the heaps in the thread cache
    if (size_classes[cls] != nullptr) runtime.heap.put_sizedpage(size_classes[cls]);
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

    void *ptr = allocate_backing_data(*m, size);

    m->set_pointer(ptr);
    return m->to_handle();
  }


  void *ThreadCache::hrealloc(void *handle, size_t new_size) {
    // TODO: There is a race here... I think its okay, as a realloc really should
    // be treated like a UAF, and ideally another thread would not access the handle
    // while it is being reallocated.

    alaska::Mapping *m = alaska::Mapping::from_handle(handle);
    void *ptr = m->get_pointer();

    auto *page = this->runtime.heap.pt.get_unaligned(ptr);

    size_t old_size = page->size_of(ptr);

    void *old_data = m->get_pointer();
    void *new_data = allocate_backing_data(*m, new_size);

    // We should copy the minimum of the two sizes between the allocations.
    size_t copy_size = old_size > new_size ? new_size : old_size;
    memcpy(new_data, old_data, copy_size);


    // Assign the handle to point to the new data
    m->set_pointer(new_data);

    // Free the original allocation
    free_allocation(*m);

    return handle;
  }

  void ThreadCache::free_allocation(const alaska::Mapping &m) {
    void *ptr = m.get_pointer();
    // Grab the page from the global heap (walk the page table).
    auto *page = this->runtime.heap.pt.get_unaligned(ptr);
    ALASKA_ASSERT(page != NULL, "calling hfree should always return a heap page");

    if (page->is_owned_by(this)) {
      log_trace("Free handle %p locally", handle);
      page->release_local(m, ptr);
    } else {
      log_trace("Free handle %p remotely", handle);
      page->release_remote(m, ptr);
    }
  }


  void ThreadCache::hfree(void *handle) {
    alaska::Mapping *m = alaska::Mapping::from_handle(handle);
    // Free the allocation behind a mapping
    free_allocation(*m);
    m->set_pointer(nullptr);
    // Return the handle to the handle table.
    this->runtime.handle_table.put(m, this);
  }


  size_t ThreadCache::get_size(void *handle) {
    alaska::Mapping *m = alaska::Mapping::from_handle(handle);
    void *ptr = m->get_pointer();
    auto *page = this->runtime.heap.pt.get_unaligned(ptr);
    if (page == nullptr) return 0;
    return page->size_of(ptr);
  }

  Mapping *ThreadCache::new_mapping(void) {
    auto m = handle_slab->alloc();

    if (unlikely(m == NULL)) {
      handle_slab = runtime.handle_table.new_slab(this);
      // This BETTER work!
      m = handle_slab->alloc();
    }

    return m;
  }

}  // namespace alaska
