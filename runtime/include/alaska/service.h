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


#include <alaska/internal.h>
#include <stdbool.h>

// This file contains the interface that a service must implement to be a
// complete service under Alaska.

// Initialize whatever state the service needs.
extern void alaska_service_init(void);
// Deinitialize whatever state the service needs.
extern void alaska_service_deinit(void);

// Allocation API for the personalities to implement.
// `alloc` in this context acts as both a malloc and a realloc call. If `ent->ptr` is
// non-null, it is a realloc request. If it is null, it is a malloc request.
// Whatever the implementation does, ent->ptr must be valid (and at least `new_size` bytes long)
// when this function returns. `ent->size` must also be updated.
extern void alaska_service_alloc(alaska_mapping_t *ent, size_t new_size);
// free `ent->ptr` which was previously allocated by the service.
extern void alaska_service_free(alaska_mapping_t *ent);
extern void alaska_service_barrier(void);
extern size_t alaska_service_usable_size(void *ptr);

extern void alaska_service_commit_lock_status(alaska_mapping_t *ent, bool locked);

extern void alaska_service_swap_in(alaska_mapping_t *ent);