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
    log_trace("alloc: size = %zu", size);
    // In the fast path, this allocation routine is *just* a linked list pop.
    // In the slow path, we must extend the free list by either bump allocating
    // many free blocks *or* by swapping the remote free list.
    Block *b = this->local_free;
    log_trace("alloc local_free = %p", this->local_free);

    if (unlikely(b == nullptr)) {
      // If there was nothing on the local_free list, fall back to slow allocation :(
      return alloc_slow(m, size);
    }

    oid_t oid = this->object_to_oid(b);
    Header *h = this->oid_to_header(oid);
    printf("h = %p, b = %p, oid = %d\n", h, b, oid);

    // Pop the entry
    this->local_free = b->next;
    b->next = nullptr;  // Zero out the block part of the object (SEC?)

    h->mapping = const_cast<alaska::Mapping *>(&m);

    live_objects++;
    return (void *)b;
  }


  void *SizedPage::alloc_slow(const alaska::Mapping &m, alaska::AlignedSize size) {
    // TODO: this number is random! Maybe there is a better way of chosing this.
    long extended_count = extend(128);

    log_trace("alloc: extended_count = %ld", extended_count);


    // 1. If we managed to extend the list, return one of the blocks from it.
    if (extended_count > 0) {
      // Fall back into the alloc function to do the heavy lifting of actually allocating
      // one of the blocks we just extended the list with.
      return alloc(m, size);
    }


    // 2. If the list was not extended, try swapping the remote_free list and the local_free list.
    // This is a little tricky because we need to worry about atomics here.
    do {
      log_trace("alloc: local_free = %p, remote_free = %p\n", local_free, remote_free);
      this->local_free = this->remote_free;
    } while (!__atomic_compare_exchange_n(
        &this->remote_free, &this->local_free, nullptr, 1, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED));


    // If local-free is still null, return null. otherwise, fall back to alloc.
    if (this->local_free == nullptr) {
      return nullptr;
    }
    return alloc(m, size);
  }


  bool SizedPage::release_local(alaska::Mapping &m, void *ptr) {
    printf("local free of %p\n", ptr);
    oid_t oid = object_to_oid(ptr);
    auto *h = oid_to_header(oid);
    h->mapping = nullptr;
    live_objects--;  // TODO: ATOMICS


    Block *blk = static_cast<Block *>(ptr);
    // Free the object by pushing it onto the local free list (no atomics needed!)
    blk->next = local_free;
    local_free = blk;

    return true;
  }


  bool SizedPage::release_remote(alaska::Mapping &m, void *ptr) {
    printf("remote free of %p\n", ptr);
    oid_t oid = object_to_oid(ptr);
    auto *h = oid_to_header(oid);
    h->mapping = nullptr;

    live_objects--;  // TODO: ATOMICS!

    // Free the object by pushing it, atomically onto the remote free list
    Block *blk = static_cast<Block *>(ptr);
    atomic_block_push(&remote_free, blk);

    return true;
  }


  long SizedPage::extend(long count) {
    long e = 0;
    for (; e < count && bump_next != capacity; e++) {
      auto b = (Block *)oid_to_object(bump_next++);
      b->next = this->local_free;
      this->local_free = b;
    }

    return e;
  }




  void SizedPage::set_size_class(int cls) {
    size_class = cls;

    size_t object_size = alaska::class_to_size(cls);

    capacity =
        alaska::page_size / round_up(object_size + sizeof(SizedPage::Header), alaska::alignment);

    if (capacity == 0) {
      log_warn(
          "SizedPage allocated with an object which was too large (%zu bytes in a %zu byte page. "
          "max object = %zu). No capacity!",
          object_size, alaska::page_size, alaska::max_object_size);
    } else {
      log_trace("set_size_class(%d). os=%zu, cap=%zu", cls, object_size, capacity);
    }


    headers = (SizedPage::Header *)memory;
    objects = (Block *)round_up((uintptr_t)headers + capacity, alaska::alignment);
    log_info("cls = %-2d, memory = %p, headers = %p, objects = %p", cls, memory, headers, objects);


    live_objects = 0;
    bump_next = 0;
  }




  void SizedPage::defragment(void) {
    //
  }


}  // namespace alaska
