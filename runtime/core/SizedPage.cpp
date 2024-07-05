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

#include <alaska/SizedPage.hpp>
#include <alaska/SizeClass.hpp>
#include <alaska/Logger.hpp>
#include <string.h>

namespace alaska {


  void *SizedPage::alloc(const alaska::Mapping &m, alaska::AlignedSize size) {
    // In the fast path, this allocation routine is *just* a linked list pop.
    // In the slow path, we must extend the free list by either bump allocating
    // many free blocks *or* by swapping the remote free list.
    void *o = free_list.pop();

    if (unlikely(o == nullptr)) {
      // If there was nothing on the local_free list, fall back to slow allocation :(
      return alloc_slow(m, size);
    }


    // All paths for allocation go through this path.
    oid_t oid = this->object_to_oid(o);
    Header *h = this->oid_to_header(oid);
    h->mapping = const_cast<alaska::Mapping *>(&m);

    live_objects++;
    return o;
  }


  void *SizedPage::alloc_slow(const alaska::Mapping &m, alaska::AlignedSize size) {
    // TODO: this number is random! Maybe there is a better way of chosing this.
    long extended_count = extend(64);
    log_trace("alloc: extended_count = %ld", extended_count);

    // 1. If we managed to extend the list, return one of the blocks from it.
    if (extended_count > 0) {
      // Fall back into the alloc function to do the heavy lifting of actually allocating
      // one of the blocks we just extended the list with.
      return alloc(m, size);
    }

    // 2. If the list was not extended, try swapping the remote_free list and the local_free list.
    // This is a little tricky because we need to worry about atomics here.
    free_list.swap();
    // If local-free is still null, return null
    if (not free_list.has_local_free()) return nullptr;

    // Otherwise, fall back to alloc
    return alloc(m, size);
  }


  bool SizedPage::release_local(alaska::Mapping &m, void *ptr) {
    // printf("release local\n");
    oid_t oid = object_to_oid(ptr);


    auto *h = oid_to_header(oid);
    h->mapping = nullptr;
    live_objects--;  // TODO: ATOMICS

    free_list.free_local(ptr);

    return true;
  }


  bool SizedPage::release_remote(alaska::Mapping &m, void *ptr) {
    oid_t oid = object_to_oid(ptr);
    auto *h = oid_to_header(oid);
    h->mapping = nullptr;

    live_objects--;  // TODO: ATOMICS!

    free_list.free_remote(ptr);

    return true;
  }


  long SizedPage::extend(long count) {
    long e = 0;
    for (; e < count && bump_next != capacity; e++) {
      auto o = oid_to_object(bump_next++);
      free_list.free_local(o);
    }

    return e;
  }


  void SizedPage::set_size_class(int cls) {
    size_class = cls;

    size_t object_size = alaska::class_to_size(cls);
    this->object_size = object_size;

    capacity = (double)alaska::page_size /
               (double)(round_up(object_size, alaska::alignment) + sizeof(SizedPage::Header));

    if (capacity == 0) {
      log_warn(
          "SizedPage allocated with an object which was too large (%zu bytes in a %zu byte page. "
          "max object = %zu). No capacity!",
          object_size, alaska::page_size, alaska::max_object_size);
    } else {
      log_trace("set_size_class(%d). os=%zu, cap=%zu", cls, object_size, capacity);
    }


    // Headers live first
    headers = (SizedPage::Header *)memory;
    // Then, objects are placed later.
    objects = (Block *)round_up((uintptr_t)(headers + capacity), alaska::alignment);


    log_info("cls = %-2d, memory = %p, headers = %p, objects = %p", cls, memory, headers, objects);


    // initialize
    live_objects = 0;
    bump_next = 0;


    // Extend the free list
    this->extend(128);
  }


  void SizedPage::defragment(void) {
    //
  }


  void SizedPage::validate(void) {
    // alaska::Block *b;
    // int ind;
    // ind = 0;
    // for (b = this->local_free; b != NULL; b = b->next) {
    //   ALASKA_ASSERT(contains(b),
    //       "Local free list must contains blocks that are in the page. Index %d. b=%p", ind, b);
    //   ALASKA_ASSERT(!is_header(b),
    //       "Local free list must contains blocks that are not headers. Index %d, b=%p", ind, b);
    //   ind++;
    // }

    // ind = 0;
    // for (b = this->remote_free; b != NULL; b = b->next) {
    //   ALASKA_ASSERT(contains(b),
    //       "Remote free list must contains blocks that are in the page. Index %d, b=%p", ind, b);
    //   ALASKA_ASSERT(!is_header(b),
    //       "Remote free list must contains blocks that are not headers. Index %d, b=%p", ind, b);
    //   ind++;
    // }
  }



}  // namespace alaska
