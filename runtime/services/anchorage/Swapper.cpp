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
#include <map>
#include <string.h>

std::map<alaska::Mapping *, std::pair<void *, size_t>> swapped_out;

extern "C" void alaska_service_swap_in(alaska_mapping_t *m) {
  anchorage::swap_in(*m);
}


void anchorage::swap_in(alaska::Mapping &m) {
  // printf("swap in %p\n", &m);
  // printf("  ptr=%p\n", m.ptr);
  // printf("  swap.id=%zu\n", m.swap.id);

  auto f = swapped_out.find(&m);
  if (f == swapped_out.end()) {
    fprintf(stderr, "Attempt to swap in handle that wasn't swapped out.\n");
    abort();
  }

  auto ptr = f->second.first;
  auto size = f->second.second;
  void *x = anchorage::alloc(m, size);
  memcpy(x, ptr, size);
  swapped_out.erase(f);
}


void anchorage::swap_out(alaska::Mapping &m) {
  void *original_ptr = m.ptr;
  // copy the data elsewhere.
  void *new_ptr = ::malloc(m.size);
  memcpy(new_ptr, original_ptr, m.size);

  swapped_out[&m] = std::make_pair(new_ptr, m.size);

  // free the backing memory for this handle
  anchorage::free(m, original_ptr);
  // printf("swap out %p to %p\n", &m, new_ptr);
  // m.swap.id = 42;
}

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
