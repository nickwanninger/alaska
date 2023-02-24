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

#include "./allocator.h"
#include <alaska/internal.h>


void anchorage::Defragmenter::add_chunk(anchorage::Chunk &chunk) {
  // Just insert it into the set
  chunks.insert(&chunk);
}


void anchorate::Defragmenter::run(void) {
  //
}