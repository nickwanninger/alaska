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
  struct Runtime;


  class Localizer {
    alaska::Runtime &rt;

   public:
    Localizer(alaska::Configuration &config, alaska::Runtime &rt);
    void feed_hotness_data(size_t count, handle_id_t *handle_ids);
  };
}  // namespace alaska
