
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


#include <alaska/Runtime.hpp>
#include <alaska/Localizer.hpp>

namespace alaska {


  Localizer::Localizer(alaska::Configuration &config, alaska::Runtime &rt)
      : rt(rt) {}
  void Localizer::feed_hotness_data(size_t count, handle_id_t *handle_ids) {}
}  // namespace alaska
