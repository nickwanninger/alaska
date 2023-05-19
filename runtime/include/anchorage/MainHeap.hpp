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

  // The job of a main heap is to manage a set of SubHeaps.

  class MainHeap : public anchorage::Arena {};
}  // namespace anchorage