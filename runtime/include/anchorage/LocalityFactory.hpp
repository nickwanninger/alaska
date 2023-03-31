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


#include <alaska.h>
#include <alaska/internal.h>
#include <unordered_set>
#include <vector>

namespace anchorage {

  // This class performs all operations relating to manufacturing locality
  class LocalityFactory {
    std::unordered_set<alaska::Mapping *> reachable;
    std::vector<alaska::Mapping *> order;

    void traverse(alaska::Mapping *m);

   public:
    LocalityFactory(void *root);  // TODO: shape analysis stuff in the compiler
    void run();
  };

}  // namespace anchorage
