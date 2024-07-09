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
#include <alaska/ShardedFreeList.hpp>
#include <alaska/SizedAllocator.hpp>

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


    void defragment(void);

    // Run through the page and validate as much info as possible w/ asserts.
    void validate(void);

   private:
    struct Header {
      alaska::Mapping *mapping;
    };

    long header_to_ind(Header *h);
    Header *ind_to_header(long oid);
    long object_to_ind(void *ob);
    void *ind_to_object(long oid);

    ////////////////////////////////////////////////

    int size_class;      // The size class of this page
    size_t object_size;  // The byte size of the size class of this page (saves a load)
    Header *headers;     // The start of the headers
    void *objects;
    long capacity;
    long live_objects;

    SizedAllocator allocator;
  };


  inline long SizedPage::header_to_ind(Header *h) { return (h - headers); }
  inline SizedPage::Header *SizedPage::ind_to_header(long oid) { return headers + oid; }
  inline long SizedPage::object_to_ind(void *ob) {
    return ((uintptr_t)ob - (uintptr_t)objects) / this->object_size;
  }
  inline void *SizedPage::ind_to_object(long oid) {
    return (void *)((uintptr_t)objects + oid * this->object_size);
  }

}  // namespace alaska
