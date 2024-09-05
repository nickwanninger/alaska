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
  class LocalityPage final : public alaska::HeapPage {
   public:
    struct Metadata {
      alaska::Mapping *mapping;
    };

    LocalityPage(void *backing_memory) : alaska::HeapPage(backing_memory) {
      data = backing_memory;
      data_bump_next = data;
      md_bump_next = get_md(0);
    }

    ~LocalityPage() override;

    void *alloc(const alaska::Mapping &m, alaska::AlignedSize size) override;
    bool release_local(const alaska::Mapping &m, void *ptr) override;
    size_t size_of(void *) override;
    inline size_t available() const { return get_free_space(); }


   private:
    Metadata *find_md(void *ptr);
    inline Metadata *get_md(uint32_t offset) {
      return (Metadata *)((uintptr_t)data + page_size) - (offset + 1);
    }

    inline void *get_ptr(uint32_t index) { return get_mapping(index)->get_pointer(); }

    inline int num_allocated(void) { return get_md(0) - (md_bump_next); }

    inline alaska::Mapping *get_mapping(uint32_t offset) { return get_md(offset)->mapping; }

    inline size_t get_free_space() const {
      return (off_t)md_bump_next - (off_t)data_bump_next;
    }
    inline size_t used_space() const { return (off_t)data_bump_next - (off_t)data; }



    void *data = nullptr;
    void *data_bump_next = nullptr;
    Metadata *md_bump_next = nullptr;
  };
};  // namespace alaska
