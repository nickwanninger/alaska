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

#include <bits/types/FILE.h>
#include <stdlib.h>
#include <stdint.h>
#include <alaska/alaska.hpp>
#include <alaska/Logger.hpp>
#include <math.h>
#include <alaska/HeapPage.hpp>

namespace alaska {


  class VariablePage : public alaska::HeapPage {
   public:
    using Metadata = uint32_t;

    inline VariablePage() {
      data = malloc(alaska::page_size);
      data_bump_next = data;
      md_bump_next = get_md(0);
    }
    inline ~VariablePage() override { ::free(data); }



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


    bool release(alaska::Mapping &m, void *ptr) override {
      // do nothing for now.
      off_t offset = (uintptr_t)ptr - (uintptr_t)data;
      off_t capacity = (uintptr_t)data_bump_next - (uintptr_t)data;

      float loc = (float)offset / (float)capacity;
      int count = num_allocated();

      auto md = find_md(&m);


      m.reset();
      // TODO: Just return true for now...
      return true;
      // for (int i = 0; i < count; i++) {
      //   auto md = get_md(i);
      //   if (*md == m->to_compact()) {
      //     md_off = i;
      //   }
      // }

      // float md_loc = (float)md_off / (float)count;
      // // log_debug("ptr: off: %4zu, cap: %4zu, %5.2f%%", offset, capacity, loc * 100);
      // // log_debug("md:  off: %4zu, cap: %4zu, %5.2f%%", md_off, count, md_loc * 100);
      // float error = fabs((loc - md_loc));
      // log_debug("byte off: %6zu   est: %5.2f%%   act: %5.2f%%  err: %5.2f%% (max %zu entries)",
      //     offset, loc * 100, md_loc * 100, error * 100, (long)(count * error));
    }



    Metadata *find_md(alaska::Mapping *m) {
      void *ptr = m->get_pointer();
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

      int steps = 0;
      // Go left or right to find the right one.
      if (ptr > guess_ptr) {
        while (ptr > guess_ptr) {
          if (guess_off >= num_alloc) return nullptr;
          guess_off++;
          guess_ptr = get_ptr(guess_off);
          steps++;
        }
      } else {
        while (ptr < guess_ptr) {
          if (guess_off <= 0) return nullptr;
          guess_off--;
          guess_ptr = get_ptr(guess_off);
          steps++;
        }
      }

      log_debug("%p guess: %d, found: %d, steps: %d", ptr, original_guess_off, guess_off, steps);


      // {
      //   // find the real one.
      //   off_t real_off = -1;
      //   for (int i = 0; i < num_allocated(); i++) {
      //     auto md = get_md(i);
      //     if (*md == m->to_compact()) {
      //       real_off = i;
      //       break;
      //     }
      //   }

      //   auto real_md = get_md(real_off);
      //   auto guess_md = get_md(guess_off);


      //   auto real_ptr = get_ptr(real_off);
      //   auto guess_ptr = get_ptr(guess_off);
      //   off_t guess_ptr_off = (uintptr_t)real_ptr - (uintptr_t)guess_ptr;

      //   log_debug("%p g:%4zu r:%4zu n:%4zu %4zd Δ:%zd", ptr, guess_off, real_off,
      //   num_allocated(),
      //       real_off - guess_off, guess_ptr_off);
      // }


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

   public:
    void *data = nullptr;
    void *data_bump_next = nullptr;
    uint32_t *md_bump_next = nullptr;
  };
};  // namespace alaska
