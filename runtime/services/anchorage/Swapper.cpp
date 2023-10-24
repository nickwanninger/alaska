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


#include <anchorage/Swapper.hpp>
#include <alaska/alaska.hpp>
#include <alaska/service.hpp>

#include <ck/pair.h>
#include <ck/map.h>
#include <ck/lock.h>
#include <string.h>

void alaska::service::swap_in(alaska::Mapping *m) {
  anchorage::swap_in(*m);
}


void anchorage::swap_in(alaska::Mapping &m) {
  // if (m.alt.swap == 0) {
  //   return;
  // }
  // printf("Swap on %p\n", m);
  // TODO: CAS
  // m.alt.swap = 0; // unswap
  // m.ptr = (void*)m.alt.misc;
  if (!__sync_bool_compare_and_swap(&m.ptr, m.ptr, (void*)m.alt.misc)) {
    printf("CAS failed!\n");
  }
  // m.ptr = (void *)m.swap.info;
  // auto block = anchorage::Block::get(m.ptr);
  // m.size = block->size();
}


void anchorage::swap_out(alaska::Mapping &m) {
// #ifdef ALASKA_SWAP_SUPPORT
//   void *ptr = m.ptr;
//   // next translation will "fault" and figure out what to do.
//   m.ptr = NULL;
//   m.swap.flag = 1;
//   m.swap.info = (size_t)ptr;
// #endif
}
