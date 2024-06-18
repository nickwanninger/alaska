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

namespace alaska {

  // A SizedPage allocates objects of one specific size class.
  class SizedPage : public alaska::HeapPage {
  public:
    using HeapPage::HeapPage;
    inline ~SizedPage(void) override {
      // ...
    }


    void *alloc(const alaska::Mapping &m, alaska::AlignedSize size) override;
    bool release(alaska::Mapping &m, void *ptr) override;

    // How many free slots are there?
    long available(void) { return 0; }


    void set_size_class(int cls);
    int get_size_class(void) const { return size_class; }


  private:

    struct Header {
      void *pointer;
    };
    ////////////////////////////////////////////////



    int size_class;
    long live_objects;
    long capacity;

    Header *headers;
    void *allocation_start;


    Header *bump_next;
  };


}  // namespace alaska
