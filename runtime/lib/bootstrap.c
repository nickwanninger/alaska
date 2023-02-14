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


ALASKA_INLINE void *alaska_lock_bootstrap(void *restrict ptr) {
  handle_t h;
  h.ptr = ptr;
  if (unlikely(h.flag == 0)) {
    return ptr;
  }
  alaska_mapping_t *m = (alaska_mapping_t *)(uint64_t)h.handle;
	// naive, no tracking
  return (void *)((uint64_t)m->ptr + h.offset);
}

ALASKA_INLINE void alaska_unlock_bootstrap(void *restrict ptr) {
	// intentially a no-op
}
