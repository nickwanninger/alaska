/*
 * this file is part of the alaska handle-based memory management system
 *
 * copyright (c) 2023, nick wanninger <ncw@u.northwestern.edu>
 * copyright (c) 2023, the constellation project
 * all rights reserved.
 *
 * this is free software.  you are permitted to use, redistribute,
 * and modify it as specified in the file "license".
 */

#include <alaska.h>
#include <alaska/internal.h>
#include <alaska/service/anchorage.h>
#include <alaska/vec.h>
#include "./internal.h"
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
