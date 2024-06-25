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

#include <alaska/HeapPage.hpp>
#include <alaska/SizeClass.hpp>

namespace alaska {
  struct Block;

  // A SizedPage allocates objects of one specific size class.
  class SizedPage : public alaska::HeapPage {
   public:
    using HeapPage::HeapPage;
    inline ~SizedPage(void) override {
      // ...
    }


    void *alloc(const alaska::Mapping &m, alaska::AlignedSize size) override;
    bool release_local(alaska::Mapping &m, void *ptr) override;
    bool release_remote(alaska::Mapping &m, void *ptr) override;

    // How many free slots are there?
    long available(void) { return capacity - live_objects; }


    void set_size_class(int cls);
    int get_size_class(void) const { return size_class; }


   private:
    using oid_t = uint64_t;
    struct Header {
      alaska::Mapping *mapping;
    };

    oid_t header_to_oid(Header *h);
    Header *oid_to_header(oid_t oid);
    oid_t object_to_oid(void *ob);
    void *oid_to_object(oid_t oid);

    // Extend the free list by bump allocating
    long extend(long count);



    // Perform a slow allocation (bump allocation)
    void *alloc_slow(const alaska::Mapping &m, alaska::AlignedSize size);

    ////////////////////////////////////////////////

    oid_t bump_next;
    int size_class;// The size class of this page
    long live_objects; // The number of live objects right now
    long capacity; // How many objects can be allocated total
    Header *headers; // The start of the headers
    Block *objects;                // The start of the blocks
    Block *local_free = nullptr;   // Local free list
    Block *remote_free = nullptr;  // Remote (Delayed) free list
  };


  inline SizedPage::oid_t SizedPage::header_to_oid(Header *h) { return (h - headers); }
  inline SizedPage::Header *SizedPage::oid_to_header(oid_t oid) { return headers + oid; }
  inline SizedPage::oid_t SizedPage::object_to_oid(void *ob) {
    return ((uintptr_t)ob - (uintptr_t)objects) / alaska::class_to_size(size_class);
  }
  inline void *SizedPage::oid_to_object(oid_t oid) {
    return (void *)((uintptr_t)objects + oid * alaska::class_to_size(size_class));
  }


}  // namespace alaska
