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


// This file represents a service that doesn't do anything. It can be used
// As a stepping stone for implementing new services.
#include <alaska/alaska.hpp>
#include <stdint.h>
#include <alaska/service.hpp>
#include <malloc.h>


size_t alaska::service::usable_size(void *ptr) {
  return malloc_usable_size(ptr);
}


void alaska::service::swap_in(alaska::Mapping *m) {
  return;
}


void alaska::service::init(void) {
  // None...
}

void alaska::service::deinit(void) {
  // None...
}


void alaska::service::alloc(alaska::Mapping *ent, size_t new_size) {
  // Realloc handles the logic for us. (If the first arg is NULL, it just allocates)
  ent->ptr = realloc(ent->ptr, new_size);
}

// free `ent->ptr` which was previously allocated by the service.
void alaska::service::free(alaska::Mapping *ent) {
  ::free(ent->ptr);
  ent->ptr = NULL;
}

void alaska::service::barrier(void) {
  // Do nothing.
}


extern void alaska::service::commit_lock_status(alaska::Mapping *ent, bool locked) {
  // do nothing
}
