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

#include <alaska/heaps/HeapPage.hpp>
#include <alaska/AllocationRequest.hpp>
#include <alaska/core/ThreadCache.hpp>
#include <alaska/core/Runtime.hpp>

#include <sys/mman.h>

namespace alaska {

  HeapPage::~HeapPage() {}

  HeapPage::HeapPage(void *backing_memory)
      : memory(backing_memory) {
    // The first thing we do is store the back pointer to ourselves in the backing memory.
    header()->owner = this;
    header()->magic = HeapPageHeader::expected_magic;
    header()->end = (char *)memory + page_size;
    // alaska::printf("HeapPage: memory: %p, header %p, owned by %p\n", memory,
    // (uintptr_t)this->header(), this->header()->owner);
    mag_list = LIST_HEAD_INIT(mag_list);
    tc_list = LIST_HEAD_INIT(tc_list);
    age_list = LIST_HEAD_INIT(age_list);
  }


  void *HeapPage::allocate_handle(const AllocationRequest &req) {
    alaska::Mapping *m = req.requestor.new_mapping();
    void *ptr = this->alloc(*m, req.size);
    // If the allocation request failed, make sure to free the handle too!
    if (ptr == NULL) {
      req.requestor.free_mapping(m);
      return NULL;
    }

    // Now that we have data and a handle, zero it if we need to
    if (req.zero) memset(ptr, 0, req.size);
    // And set the mapping to point to the data.
    m->set_pointer(ptr);
    return m->to_handle(0);
  }

  void *HeapPage::alloc(const Mapping &m, AlignedSize size) { return nullptr; }



}  // namespace alaska
