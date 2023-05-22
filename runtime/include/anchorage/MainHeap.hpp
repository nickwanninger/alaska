/*
 * This file is part of the Alaska Handle-Based Memory Management System
 *
 * Copyright (c) 2023, Nick Wanninger <ncw@u.northwestern.edu>
 * Copyright (c) 2023, The Constellation Project
 * All rights reserved.
 *
 * This is free software.  You are permitted to use, redistribute,
 * and modify it as specified in the file "LICENSE".
 */
#pragma once

#include <anchorage/LinkedList.h>
#include <anchorage/SubHeap.hpp>
#include <anchorage/Arena.hpp>

#include <stdlib.h>
#include <assert.h>

// Pull in heaplayers for the main heap's backing data source
#include <heaplayers.h>



namespace anchorage {

  // The job of a main heap is to manage a set of SubHeaps, and
  // send allocation requests to the relevant SubHeap according
  // to the requested size.
  class MainHeap : public anchorage::Arena {
    typedef anchorage::Arena SuperHeap;

   public:
    enum { Alignment = 16 };

    MainHeap()
        : SuperHeap() {
    }

    void *alloc(alaska::Mapping &m, size_t size);
    void free(alaska::Mapping &m);

	 private:
  };



  // Allocate some memory into a mapping
  inline void *MainHeap::alloc(alaska::Mapping &m, size_t size) {
    // TODO:
    m.ptr = nullptr;
    return m.ptr;
  }


  // Free the memory associated with a mapping.
  inline void MainHeap::free(alaska::Mapping &m) {
    void *ptr = m.ptr;  // Grab the pointer
    (void)ptr;
  }

}  // namespace anchorage
