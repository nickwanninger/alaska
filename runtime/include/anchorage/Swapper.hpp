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
#include <alaska/alaska.hpp>
#include <ck/box.h>
#include <ck/template_lib.h>

namespace anchorage {

  // Someone else can ask us to swap a hande in our out
  void swap_in(alaska::Mapping &m);
  void swap_out(alaska::Mapping &m);

}  // namespace anchorage