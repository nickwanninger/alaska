/*
 * This file is part of the Alaska Handle-Based Memory Management System
 *
 * Copyright (c) 2024, Nick Wanninger <ncw@u.northwestern.edu>
 * Copyright (c) 2024, The Constellation Project
 * All rights reserved.
 *
 */
/** This is free software.  You are permitted to use, redistribute,
 * and modify it as specified in the file "LICENSE".
 */

#include <alaska/SizedPage.hpp>
#include <alaska/SizeClass.hpp>
#include <alaska/Logger.hpp>
#include <alaska/SizedAllocator.hpp>
#include <string.h>

namespace alaska {


  SizedPage::~SizedPage() { return; }

  void *SizedPage::alloc(const alaska::Mapping &m, alaska::AlignedSize size) {
    void *o = allocator.alloc();
    if (unlikely(o == nullptr)) return nullptr;

    // All paths for allocation go through this path.
    long oid = this->object_to_ind(o);
    Header *h = this->ind_to_header(oid);
    h->set_mapping(const_cast<alaska::Mapping *>(&m));
    h->size_slack = this->object_size - size;
    return o;
  }


  bool SizedPage::release_local(const alaska::Mapping &m, void *ptr) {
    long oid = object_to_ind(ptr);
    auto *h = ind_to_header(oid);
    h->set_mapping(nullptr);
    allocator.release_local(ptr);
    return true;
  }


  bool SizedPage::release_remote(const alaska::Mapping &m, void *ptr) {
    auto oid = object_to_ind(ptr);
    auto *h = ind_to_header(oid);
    h->set_mapping(nullptr);
    allocator.release_remote(ptr);
    return true;
  }



  void SizedPage::set_size_class(int cls) {
    size_class = cls;

    size_t object_size = alaska::class_to_size(cls);
    this->object_size = object_size;

    capacity = (double)alaska::page_size /
               (double)(round_up(object_size, alaska::alignment) + sizeof(SizedPage::Header));
    live_objects = 0;

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
    allocator.configure(objects, object_size, capacity);
  }


  void SizedPage::compact(void) {}



  void SizedPage::validate(void) {}

}  // namespace alaska
