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


namespace alaska {


  void *SizedPage::alloc(const alaska::Mapping &m, alaska::AlignedSize size) {
    // If we cannot allocate an object, return null
    if (available() == 0 || bump_next == capacity) {
      log_trace("Could not allocate object from sized heap! avail=%zu", available());
      return nullptr;
    }


    // This functionality does not have to be thread safe
    oid_t oid = bump_next++;

    live_objects++;

    auto *h = oid_to_header(oid);
    auto *o = oid_to_object(oid);


    h->mapping = const_cast<alaska::Mapping*>(&m);

    return o;
  }


  bool SizedPage::release_local(alaska::Mapping &m, void *ptr) {
    // we are trusting it is aligned...
    // TODO: security!
    oid_t oid = object_to_oid(ptr);
    auto *h = oid_to_header(oid);
    h->mapping = nullptr;
    live_objects--; // TODO: Free list!

    // Don't free, yet
    return true;
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
    objects = (void *)round_up((uintptr_t)headers + capacity, alaska::alignment);
    log_info("cls = %-2d, memory = %p, headers = %p, objects = %p", cls, memory, headers,
             objects);


    live_objects = 0;
    bump_next = 0;
  }



}  // namespace alaska
