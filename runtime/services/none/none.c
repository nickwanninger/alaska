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
#include <alaska/personality/none.h>
#include <alaska/internal.h>
#include <stdint.h>
#include <alaska/service.h>


void alaska_service_init(void) {
	// None
}

void alaska_service_deinit(void) {
	// None...
}


void alaska_service_alloc(alaska_mapping_t *ent, size_t new_size) {
	// Realloc handles the logic for us. (If the first arg is NULL, it just allocates)
	ent->ptr = realloc(ent->ptr, new_size);
	ent->size = new_size;
}
// free `ent->ptr` which was previously allocated by the personality.
void alaska_service_free(alaska_mapping_t *ent) {
	free(ent->ptr);
	ent->ptr = NULL;
}

void alaska_service_barrier(void) {
	// Do nothing.
}


extern void alaska_service_commit_lock_status(alaska_mapping_t *ent, bool locked) {
  // do nothing
}