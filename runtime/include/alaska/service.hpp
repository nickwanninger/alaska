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


#include <alaska/alaska.hpp>

namespace alaska {
  // This file contains the interface that a service must implement to be a
  // complete service under Alaska. All of these functions are not implemented in the lib/ folder of
  // the runtime, and must be implemented by some bit of code in the relevant `services/$service`
  // folder in order to successfully compile.
  namespace service {
    // Initialize whatever state the service needs.
    extern void init(void);
    // Deinitialize whatever state the service needs.
    extern void deinit(void);
    // Allocation API for the personalities to implement.
    // `alloc` in this context acts as both a malloc and a realloc call. If `ent->ptr` is
    // non-null, it is a realloc request. If it is null, it is a malloc request.
    // Whatever the implementation does, ent->ptr must be valid (and at least `new_size` bytes long)
    // when this function returns. `ent->size` must also be updated.
    extern void alloc(alaska::Mapping *ent, size_t new_size);
    // free `ent->ptr` which was previously allocated by the service.
    extern void free(alaska::Mapping *ent);
    // Tell the service that the runtime state is ready to perform whatever
    // barrier operation it needs to do.
    extern void barrier(void);
    // Ask the service how big a handle is. This is to save space
    // in the handle table, as most allocators will know this anyways.
    extern size_t usable_size(void *ptr);
    // Given a mapping, tell the service if it is locked or not
    // (used in barriers). This should be as quick as possible.
    extern void commit_lock_status(alaska::Mapping *ent, bool locked);
    // A mapping was found to be non-present (the top bit of ent->ptr is set to 1).
    // This function *must* fix that or crash.
    extern void swap_in(alaska::Mapping *ent);
  };  // namespace service
}  // namespace alaska
