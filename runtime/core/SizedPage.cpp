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
#include <ck/template_lib.h>

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
    h->size_slack = 0;
    allocator.release_local(ptr);
    return true;
  }


  bool SizedPage::release_remote(const alaska::Mapping &m, void *ptr) {
    auto oid = object_to_ind(ptr);
    auto *h = ind_to_header(oid);
    h->set_mapping(nullptr);
    h->size_slack = 0;
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
    memset(headers, 0, sizeof(SizedPage::Header) * capacity);
    // Then, objects are placed later.
    objects = (Block *)round_up((uintptr_t)(headers + capacity), alaska::alignment);


    log_info("cls = %-2d, memory = %p, headers = %p, objects = %p", cls, memory, headers, objects);

    // initialize
    allocator.configure(objects, object_size, capacity);
  }


  void SizedPage::compact(void) {}



  void SizedPage::validate(void) {}

  long SizedPage::jumble(void) {
    // printf("Jumble sized page\n");

    char buf[this->object_size];  // BAD

    // Simple two finger walk to swap every allocation
    long left = 0;
    long right = capacity - 1;
    long swapped = 0;

    while (right > left) {
      auto *lo = ind_to_header(left);
      auto *ro = ind_to_header(right);

      // printf("%ld %ld   %p %p\n", left, right, lo->get_mapping(), ro->get_mapping());

      auto *rm = ro->get_mapping();
      auto *lm = lo->get_mapping();

      if (rm == NULL or rm->is_pinned()) {
        right--;
        continue;
      }

      if (lm == NULL or lm->is_pinned()) {
        left++;
        continue;
      }


      // swap the handles!

      // Grab the two pointers
      void *left_ptr = ind_to_object(left);
      void *right_ptr = ind_to_object(right);

      // Swap their data
      memcpy(buf, left_ptr, object_size);
      memcpy(left_ptr, right_ptr, object_size);
      memcpy(right_ptr, buf, object_size);

      // Tick the fingers
      left++;
      right--;


      // Swap the mappings and whatnot
      auto lss = lo->size_slack;
      auto rss = ro->size_slack;

      rm->set_pointer(left_ptr);
      ro->set_mapping(lm);
      ro->size_slack = lss;

      lm->set_pointer(right_ptr);
      lo->set_mapping(rm);
      lo->size_slack = rss;

      swapped++;
    }

    return swapped;
  }

}  // namespace alaska
