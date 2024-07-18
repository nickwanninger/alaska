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
#include <alaska/alaska.hpp>
#include <alaska/OwnedBy.hpp>
#include <alaska/list_head.h>

namespace alaska {

  // This dictates how big a page is. Note: This must be an odd number
  // due to details about how the page table is implemented.
  static constexpr uint64_t page_shift_factor = 21;
  static constexpr size_t page_size = 1LU << page_shift_factor;
  static constexpr size_t huge_object_thresh = page_size / 4;

  // Forward Declaration
  template <typename T>
  class Magazine;
  class ThreadCache;


  // used for linked lists in HeapPage instances
  struct Block final {
    Block* next;
  };

  void atomic_block_push(Block** list, Block* block);



  // A super simple type-level indicator that a size is aligned to the heap's alignment
  class AlignedSize final {
    size_t size;

   public:
    AlignedSize(size_t size)
        : size((size + 15) & ~15) {}
    size_t operator*(void) { return size; }
    operator size_t(void) { return size; }
    AlignedSize operator+(size_t other) { return size + other; }
  };




  // This class is the base-level class for a heap page. A heap page is a
  // single contiguous block of memory that is managed by some policy.
  class HeapPage : public alaska::OwnedBy<ThreadCache> {
   public:
    HeapPage(void* backing_memory);
    virtual ~HeapPage() {}

    // The size argument is already aligned and rounded up to a multiple of the rounding size.
    // Returns the data allocated, or NULL if it couldn't be.
    virtual void* alloc(const Mapping& m, AlignedSize size) = 0;
    virtual bool release_local(const Mapping& m, void* ptr) = 0;
    virtual bool release_remote(const Mapping& m, void* ptr) { return release_local(m, ptr); }
    // return the size of an object
    virtual size_t size_of(void* ptr) = 0;
    inline bool contains(void* ptr) const;


    void* start(void) const { return memory; }
    void* end(void) const { return (void*)((uintptr_t)memory + page_size); }

   protected:
    // This is the backing memory for the page. it is alaska::page_size bytes long.
    void* memory = nullptr;

   public:
    // Intrusive linked list for magazine membership
    struct list_head mag_list;
  };


  inline bool HeapPage::contains(void* pt) const {
    uintptr_t ptr = reinterpret_cast<uintptr_t>(pt);
    uintptr_t start = reinterpret_cast<uintptr_t>(memory);
    uintptr_t end = start + alaska::page_size;
    return ptr >= start && ptr < end;
  }

}  // namespace alaska
