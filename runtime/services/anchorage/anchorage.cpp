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

#include <alaska.h>
#include <alaska/internal.h>
#include <alaska/service/anchorage.h>
#include <alaska/vec.h>
#include <string.h>
#include <stdbool.h>
#include <sys/mman.h>

#include "allocator.h"

uint64_t next_last_access_time = 0;


#define DEBUG(...)


extern "C" void alaska_service_barrier(void) {
  // defer to anchorage
  anchorage::barrier();
}

extern "C" void alaska_service_alloc(alaska::Mapping *ent, size_t new_size) {
  // defer to anchorage
  anchorage::alloc(*ent, new_size);
}


extern "C" void alaska_service_free(alaska::Mapping *ent) {
  // defer to anchorage
  anchorage::free(*ent, ent->ptr);
}


extern "C" void alaska_service_init(void) {
  // defer to anchorage
  anchorage::allocator_init();
}

extern "C" void alaska_service_deinit(void) {
  // defer to anchorage
  anchorage::allocator_deinit();
}



size_t anchorage::moved_bytes = 0;
void *anchorage::memmove(void *dst, void *src, size_t size) {
  anchorage::moved_bytes += size;
  return ::memmove(dst, src, size);
}