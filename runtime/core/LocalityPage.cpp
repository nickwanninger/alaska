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

#include <alaska/LocalityPage.hpp>



namespace alaska {


  LocalityPage::~LocalityPage() {}

  void *LocalityPage::alloc(const alaska::Mapping &m, alaska::AlignedSize size) {
    log_debug("VariablePage: Alloc %zu into %p", size, &m);
    log_debug(" free space: %zu", get_free_space());

    if (get_free_space() < size) return nullptr;

    void *data = data_bump_next;
    log_trace("data %p", data);
    data_bump_next = (void *)((uintptr_t)data_bump_next + size);
    log_trace("new bump %p", data_bump_next);

    auto md = md_bump_next;
    log_trace("md %p", md);
    md_bump_next--;


    log_trace("setting md to %08lx", m.to_compact());
    md->mapping = const_cast<alaska::Mapping *>(&m);
    log_trace("set md!");

    return data;
  }


  bool LocalityPage::release_local(alaska::Mapping &m, void *ptr) {
    // Don't do anything other than
    auto md = find_md(ptr);
    md->mapping = nullptr;
    return true;
  }
  LocalityPage::Metadata *LocalityPage::find_md(void *ptr) {
    if (ptr < data || ptr > data_bump_next) {
      log_warn("%p is not in this page.", ptr);
      return nullptr;
    }

    auto num_alloc = num_allocated();
    int scale = used_space() / num_alloc;
    off_t guess_off = ((uintptr_t)ptr - (uintptr_t)data) / scale;
    if (guess_off >= num_alloc) guess_off = num_alloc - 1;

    // Grab the guess metadata
    auto guess_ptr = get_ptr(guess_off);
    auto original_guess_off = guess_off;

    // Go left or right to find the right one.
    if (ptr > guess_ptr) {
      while (ptr > guess_ptr) {
        if (guess_off >= num_alloc) return nullptr;
        guess_off++;
        guess_ptr = get_ptr(guess_off);
      }
    } else {
      while (ptr < guess_ptr) {
        if (guess_off <= 0) return nullptr;
        guess_off--;
        guess_ptr = get_ptr(guess_off);
      }
    }

    return nullptr;
  }
}  // namespace alaska
