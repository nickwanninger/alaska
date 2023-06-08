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

#include <ck/set.h>
#include <anchorage/Anchorage.hpp>

namespace anchorage {

  /**
   * Defragmenter: As the core of the Anchorage service, this class will
   * perform functionality required to move memory around using various tracking
   * mechanisms and heuristics.
   */
  class Defragmenter {
   private:
    bool can_move(anchorage::Block *free_block, anchorage::Block *to_move);
    int perform_move(anchorage::Chunk &chunk, anchorage::Block *free_block, anchorage::Block *to_move);
    int naive_compact(anchorage::Chunk &chunk);

   public:
    int run(const ck::set<anchorage::Chunk *> &chunks);
  };
}  // namespace anchorage
