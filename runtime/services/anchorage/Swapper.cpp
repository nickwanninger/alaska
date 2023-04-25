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

#include <anchorage/Block.hpp>
#include <anchorage/Chunk.hpp>
#include <anchorage/Swapper.hpp>
#include <alaska/internal.h>
#include <ck/pair.h>
#include <ck/map.h>
#include <ck/lock.h>
#include <string.h>

static uint64_t next_swapid = 0;
ck::mutex swap_mutex;
ck::map<uint64_t, ck::pair<void *, size_t>> swapped_out;


extern "C" void alaska_service_swap_in(alaska_mapping_t *m) {
  anchorage::swap_in(*m);
}


void anchorage::swap_in(alaska::Mapping &m) {
  ck::scoped_lock l(swap_mutex);

  auto f = swapped_out.find(m.swap.id);
  if (f == swapped_out.end()) {
    fprintf(stderr, "Attempt to swap in handle that wasn't swapped out.\n");
    abort();
  }


  auto ptr = f->value.first;
  auto size = f->value.second;
  void *x = anchorage::alloc(m, size);
  memcpy(x, ptr, size);
  swapped_out.remove(f);
}


void anchorage::swap_out(alaska::Mapping &m) {
  ck::scoped_lock l(swap_mutex);

  void *original_ptr = m.ptr;
  // copy the data elsewhere.
  void *new_ptr = ::malloc(m.size);
  memcpy(new_ptr, original_ptr, m.size);

  auto id = next_swapid++;
  swapped_out[id] = ck::pair(new_ptr, m.size);

  // free the backing memory for this handle
  anchorage::free(m, original_ptr);
  m.swap.id = id;
}

anchorage::MMAPSwapDevice::~MMAPSwapDevice() {
}

// Force a mapping to be swapped out
bool anchorage::MMAPSwapDevice::swap_in(alaska::Mapping &m) {
  return true;
}


bool anchorage::MMAPSwapDevice::swap_out(alaska::Mapping &m) {
  return true;
}
