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
    if (available() == 0) return nullptr;


    // TODO:
    return nullptr;
  }


  bool SizedPage::release(alaska::Mapping &m, void *ptr) {
    // Don't free, yet
    return true;
  }




  void SizedPage::set_size_class(int cls) {
    size_class = cls;

    size_t object_size = alaska::class_to_size(cls);

    capacity = alaska::page_size / round_up(object_size + sizeof(SizedPage::Header), alaska::alignment);

    if (capacity == 0) {
      log_warn("SizedPage allocated with an object which was too large (%zu bytes in a %zu byte page. max object = %zu). No capacity!", object_size, alaska::page_size, alaska::max_object_size);
    } else {
      log_trace("set_size_class(%d). os=%zu, cap=%zu", cls, object_size, capacity);
    }


    headers = (SizedPage::Header*)memory;
    allocation_start = (void*)round_up((uintptr_t)headers + capacity, alaska::alignment);
    log_info("cls = %-2d, memory = %p, headers = %p, objects = %p", cls, memory, headers, allocation_start);
  }



}  // namespace alaska
