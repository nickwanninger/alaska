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
#include <anchorage/Anchorage.hpp>
#include <anchorage/SubHeap.hpp>
#include <anchorage/Arena.hpp>
#include "heaps/top/mmapheap.h"

#include <stdlib.h>
#include <assert.h>

// Pull in heaplayers for the main heap's backing data source
#include <heaplayers.h>



namespace anchorage {


  class Arena : public HL::SizedMmapHeap {
    typedef HL::SizedMmapHeap SuperHeap;

   public:
    enum { Alignment = anchorage::page_size };

    explicit Arena();
    inline bool contains(const void *ptr) const {
      auto arena = (uintptr_t)(m_start);
      auto ptrval = (uintptr_t)(ptr);
      return arena <= ptrval && ptrval < arena + anchorage::kArenaSize;
    }


   private:
    void *m_start;
  };



  inline anchorage::Arena::Arena() {
    //
  }
}  // namespace anchorage