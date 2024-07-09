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

#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <alaska/alaska.hpp>
#include <alaska/Logger.hpp>
#include <alaska/HeapPage.hpp>

namespace alaska {


  // A locality page is meant to strictly bump allocate objects of variable size in order
  // of their expected access pattern in the future. It's optimized for moving objects into
  // this page, and the expected lifetimes of these objects is long enough that we don't really
  // care about freeing or re-using the memory occupied by them when they are gone.
  class LocalityPage : public alaska::HeapPage {
   public:
    using Metadata = uint32_t;
    using HeapPage::HeapPage;
    inline ~LocalityPage() override { ::free(data); }


    void *alloc(const alaska::Mapping &m, alaska::AlignedSize size) override {
      log_debug("VariablePage: Alloc %zu into %p", size, &m);
      log_debug(" free space: %zu", get_free_space());
      if (get_free_space() < size) {
        return nullptr;
      }

      void *data = data_bump_next;
      log_trace("data %p", data);
      data_bump_next = (void *)((uintptr_t)data_bump_next + size);
      log_trace("new bump %p", data_bump_next);

      auto md = md_bump_next;
      log_trace("md %p", md);
      md_bump_next--;


      log_trace("setting md to %08lx", m.to_compact());
      *md = m.to_compact();
      log_trace("set md!");

      return data;
    }


    bool release_local(alaska::Mapping &m, void *ptr) override {
      // Don't do anything other than
      auto md = find_md(ptr);
      *md = 0;
      return true;
    }


   private:
    Metadata *find_md(void *ptr) {
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

    Metadata *get_md(uint32_t offset) {
      return (uint32_t *)((uintptr_t)data + page_size) - (offset + 1);
    }

    void *get_ptr(uint32_t index) { return get_mapping(index)->get_pointer(); }

    int num_allocated(void) { return get_md(0) - (md_bump_next); }

    alaska::Mapping *get_mapping(uint32_t offset) {
      return alaska::Mapping::from_compact(*get_md(offset));
    }

    int64_t get_free_space() const { return (off_t)md_bump_next - (off_t)data_bump_next; }
    int64_t used_space() const { return (off_t)data_bump_next - (off_t)data; }

    void *data = nullptr;
    void *data_bump_next = nullptr;
    uint32_t *md_bump_next = nullptr;
  };
};  // namespace alaska
