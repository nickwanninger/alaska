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
#include <alaska/barrier.hpp>
#include <alaska/service.hpp>

#include <stdint.h>
#include <malloc.h>
#include <unistd.h>

size_t alaska::service::usable_size(void *ptr) {
  return malloc_usable_size(ptr);
}


void alaska::service::swap_in(alaska::Mapping *m) {
  return;
}

pthread_t anchorage_barrier_thread;
pthread_t anchorage_logger_thread;
static void *barrier_thread_fn(void *) {
  alaska_thread_state.escaped = 1;
  while (1) {
    usleep(10 * 1000);
    auto start = alaska_timestamp();
    alaska::barrier::begin();
    auto end = alaska_timestamp();
    printf("barrier in %lf ms\n", (end - start) / 1000.0 / 1000.0);
    alaska::barrier::end();
    // Swap the spaces and switch to waiting.
    // anchorage::Chunk::swap_spaces();
  }
  return NULL;
}



void alaska::service::init(void) {
}

void alaska::service::deinit(void) {
  // None...
}


void alaska::service::alloc(alaska::Mapping *ent, size_t new_size) {
  // Realloc handles the logic for us. (If the first arg is NULL, it just allocates)
  ent->set_pointer(realloc(ent->get_pointer(), new_size));
}

// free `ent->ptr` which was previously allocated by the service.
void alaska::service::free(alaska::Mapping *ent) {
  ::free(ent->get_pointer());
  ent->reset();
}

void alaska::service::barrier(void) {
  // Do nothing.
}


extern void alaska::service::commit_lock_status(alaska::Mapping *ent, bool locked) {
  // do nothing
}
