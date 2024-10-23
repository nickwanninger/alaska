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

#include <alaska/alaska.hpp>
#include <alaska/Configuration.hpp>

namespace alaska {


  // fwd decl
  class ThreadCache;


  class Localizer {
    alaska::ThreadCache &tc;

   public:
    Localizer(alaska::Configuration &config, alaska::ThreadCache &tc);

    // Get a hotness buffer that can fit `count` handle_ids.
    handle_id_t *get_hotness_buffer(size_t count);

    // Give a hotness buffer back to the localizer, filled with `count` handle ids
    void feed_hotness_buffer(size_t count, handle_id_t *handle_ids);
  };
}  // namespace alaska
