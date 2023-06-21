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
#include <anchorage/SizeMap.hpp>
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
    SubHeap *find_available_subheap(size_t requested_size);
    ck::vec<SubHeap *> subheaps[anchorage::SizeMap::num_size_classes];
  };



  // Allocate some memory into a mapping
  inline void *MainHeap::alloc(alaska::Mapping &m, size_t size) {
    SubHeap *sh = find_available_subheap(size);
		m.ptr = sh->alloc(m);
		return m.ptr;
  }


  // Free the memory associated with a mapping.
  inline void MainHeap::free(alaska::Mapping &m) {
    void *ptr = m.ptr;  // Grab the pointer
    (void)ptr;
  }


  inline anchorage::SubHeap *MainHeap::find_available_subheap(size_t requested_size) {
    anchorage::SubHeap *sh = nullptr;
    const size_t objectClass = anchorage::SizeMap::SizeClass(requested_size);
    const size_t objectSize = anchorage::SizeMap::class_to_size(objectClass);

    // printf("%zu -> c:%zu s:%zu\n", requested_size, objectClass, objectSize);

    // Find a subheap with at least one object free
    for (auto *cursor : subheaps[objectClass]) {
      if (cursor->num_free() > 0) {
        sh = cursor;
        break;
      }
    }

    if (sh == nullptr) {
      // make one!
      anchorage::Extent ext(0, 0);
      int pagecount = max(1LU, (objectSize * 256) / 4096);
      void *page = palloc(ext, pagecount);
      // printf("allocate subheap at %p\n", page);
      // Allocate a new subheap!
      sh = new SubHeap((off_t)page, pagecount, objectSize, 256);
      subheaps[objectClass].push(sh);
    }

    return sh;
  }

}  // namespace anchorage
