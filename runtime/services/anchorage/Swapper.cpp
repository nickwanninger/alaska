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

#include <algorithm>
#include <anchorage/Block.hpp>
#include <anchorage/Chunk.hpp>
#include <anchorage/Swapper.hpp>
#include <alaska/internal.h>



anchorage::MMAPSwapDevice::~MMAPSwapDevice() {
  //
}

// Force a mapping to be swapped out
bool anchorage::MMAPSwapDevice::swap_in(alaska::Mapping &m) {
  return true;
}


bool anchorage::MMAPSwapDevice::swap_out(alaska::Mapping &m) {
  return true;
}
