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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "./config.h"

#include <alaska/utils.h>
#include <alaska/list_head.h>

extern void alaska_dump_backtrace(void);


namespace alaska {
  // A mapping is *just* a pointer. If that pointer has the high bit set, it is swapped out.

  struct Mapping {
    union {
      void *ptr;  // backing memory
      struct {
        uint64_t info : 63;  // Some kind of info for the swap system
        unsigned flag : 1;   // the high bit indicates this is swapped out
      } swap;
    };
  };
}  // namespace alaska


extern "C" {
// src/lock.c
void *alaska_encode(alaska::Mapping *m, off_t offset);
void *alaska_translate(void *ptr);
void alaska_release(void *ptr);

// Ensure a handle is present.
void *alaska_ensure_present(alaska::Mapping *m);
alaska::Mapping *alaska_lookup(void *ptr);
}


typedef uint32_t alaska_spinlock_t;
#define ALASKA_SPINLOCK_INIT 0
inline void alaska_spin_lock(volatile alaska_spinlock_t *lock) {
  while (__sync_lock_test_and_set(lock, 1)) {
    // spin away
  }
}
inline int alaska_spin_try_lock(volatile alaska_spinlock_t *lock) {
  return __sync_lock_test_and_set(lock, 1) ? -1 : 0;
}
inline void alaska_spin_unlock(volatile alaska_spinlock_t *lock) {
  __sync_lock_release(lock);
}


struct alaska_lock_frame {
  struct alaska_lock_frame *prev;
  uint64_t count;
  void *locked[];
};
// In barrier.c
extern __thread struct alaska_lock_frame *alaska_lock_root_chain;
