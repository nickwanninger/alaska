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
#include <alaska/AllocationRequest.hpp>
#include <alaska/alaska.hpp>
#include <alaska/util/OwnedBy.hpp>
#include "alaska/AllocationRequest.hpp"
#include <alaska/util/list_head.h>

namespace alaska {

  /**
   * Alaska's heap is broken down into "pages" which are a certain size. Each
   * page is managed by a single policy, and different pages can have different
   * policies. For example, one page might only allocate objects of a fixed size,
   * and another might allocate objects of varying sizes.
   */
  static constexpr uint64_t page_shift_factor = 18;  // 21; // 16
  static constexpr size_t page_size = 1LU << page_shift_factor;
  static constexpr size_t huge_object_thresh = 4096;

  // Forward Declaration
  class MagazineBase;
  class ThreadCache;
  class HeapPage;


  // A super simple type-level indicator tat a size is aligned to the heap's alignment
  class AlignedSize final {
    size_t size;

   public:
    AlignedSize(size_t size)
        : size((size + 15) & ~15) {}
    size_t operator*(void) { return size; }
    operator size_t(void) { return size; }
    AlignedSize operator+(size_t other) { return size + other; }
  };


  // This structure is placed at the start of each heap page in memory to identify the owner.
  struct HeapPageHeader {
    static constexpr uint64_t expected_magic = 0xDEADBEEFDEADBEEF;
    HeapPage* owner;
    uint64_t magic = expected_magic;
    uint64_t __padding;
    char* end;      // End of the usable memory in this page.
    char start[0];  // Start of the usable memory in this page.
  };
  static_assert(alignof(HeapPageHeader) != 16, "HeapPageHeader must be 16 bytes");




  // This class is the base-level class for a heap page. A heap page is a
  // single contiguous block of memory that is managed by some policy.
  class HeapPage : public alaska::OwnedBy<ThreadCache>, public alaska::PersistentAllocation {
   public:
    HeapPage(void* backing_memory);
    virtual ~HeapPage();


    // Given an allocation request, allocate a handle and the backing data and
    // return the encoded result which should be returned to the user.
    // This function has a default implementation which wraps ::alloc() for
    // subclasses which don't want to worry about allocating a handle from
    // the threadcache's handle table.
    // Returns NULL on failure.
    virtual void* allocate_handle(const AllocationRequest& req);

    // The size argument is already aligned and rounded up to a multiple of the rounding size.
    // Returns the data allocated, or NULL if it couldn't be.
    virtual void* alloc(const Mapping& m, AlignedSize size) alaska_attr_malloc;
    virtual bool release_local(const Mapping& m, void* ptr) = 0;
    virtual bool release_remote(const Mapping& m, void* ptr) { return release_local(m, ptr); }
    virtual bool should_localize_from(uint64_t current_epoch) const { return true; }
    inline bool contains(void* ptr) const;
    virtual float fragmentation(void) { return 0.0f; /*placeholder*/ }
    virtual size_t committed_bytes(void) { return header()->end - header()->start; }
    virtual size_t available(void) = 0;  // how many bytes are available for allocation?


    void* start(void) const { return memory; }
    void* end(void) const { return (void*)((uintptr_t)memory + page_size); }

    virtual void dump_html(FILE* stream) { fprintf(stream, "TODO"); }
    virtual void dump_json(FILE* stream) {
      fprintf(stream, "{\"name\": \"HeapPage\", \"objs\": \"\"}");
    }

   protected:
    // This is the backing memory for the page. it is alaska::page_size bytes long.
    void* memory = nullptr;

    inline HeapPageHeader* header(void) const { return (HeapPageHeader*)memory; }
    inline uintptr_t memory_start(void) const { return (uintptr_t)memory + sizeof(HeapPageHeader); }
    inline uintptr_t memory_end(void) const { return (uintptr_t)memory + page_size; }

   public:
    // Intrusive linked list for magazine membership

    alaska::MagazineBase* magazine = nullptr;
    struct list_head mag_list;
    struct list_head tc_list;  // Per-ThreadCache local page queue membership
    struct list_head age_list;  // Global age-ordered list membership
    uint64_t time_of_last_use = 0;  // Milliseconds from CLOCK_MONOTONIC
    bool was_full = false;
  };


  inline bool HeapPage::contains(void* pt) const {
    uintptr_t ptr = reinterpret_cast<uintptr_t>(pt);
    uintptr_t start = reinterpret_cast<uintptr_t>(memory);
    uintptr_t end = start + alaska::page_size;
    return ptr >= start && ptr < end;
  }

}  // namespace alaska
