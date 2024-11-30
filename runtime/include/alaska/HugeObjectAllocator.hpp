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
#include <alaska/list_head.h>
#include <ck/lock.h>

namespace alaska {


  // The HugeObjectAllocator can be configured with different strategies. The main one
  // uses a custom mmap based strategy to track large objects. The other option is one
  // which is backed by libc's malloc, instead
  enum class HugeAllocationStrategy {
    CUSTOM_MMAP_BACKED,
    MALLOC_BACKED,
  };
  class HugeObjectAllocator final {
   public:
    HugeObjectAllocator(HugeAllocationStrategy strat);
    ~HugeObjectAllocator();

    void* allocate(size_t size);
    // Free a huge allocation. Returns false if it was not managed by this heap.
    bool free(void* ptr);
    // Get the size of a huge allocation. Returns 0 if it was not managed by this heap.
    size_t size_of(void* ptr);

    // Check if the allocator owns a pointer
    // NOTE: this walk the `allocations` list!
    bool owns(void* ptr);

   private:
    HugeAllocationStrategy strat;
    ck::mutex m_lock;
    struct list_head allocations = LIST_HEAD_INIT(allocations);




    struct alignas(16) HugeHeader {
      struct list_head list;
      size_t mapping_size;     // the size of the mmap region
      size_t allocation_size;  // the size of the allocation.
      void* data() { return (void*)((uintptr_t)this + sizeof(HugeHeader)); }
      static HugeHeader* from_data(void* data) {
        return (HugeHeader*)((uintptr_t)data - sizeof(HugeHeader));
      }
    };

    HugeHeader* find_header(void* ptr);
  };
}  // namespace alaska
