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



#define abort()

class NoneService : public alaska::Service {
 public:
  NoneService(void)
      : alaska::Service("none") {
  }

  ~NoneService(void) override = default;

  bool alloc(alaska::Mapping *ent, size_t new_size) override {
    ent->set_pointer(::realloc(ent->get_pointer(), new_size));
    return true;
  }


  bool free(alaska::Mapping *ent) override {
    ::free(ent->get_pointer());
    ent->reset();
    return true;
  }


  ssize_t usable_size(alaska::Mapping *ent) override {
    return malloc_usable_size(ent->get_pointer());
  }
};


static NoneService the_service;

size_t alaska::service::usable_size(void *ptr) {
  abort();
  return malloc_usable_size(ptr);
}


void alaska::service::swap_in(alaska::Mapping *m) {
  abort();
  return;
}


void alaska::service::init(void) {
  alaska::register_service(the_service);
}

void alaska::service::deinit(void) {
  // None...
}


void alaska::service::alloc(alaska::Mapping *ent, size_t new_size) {
  abort();
  // Realloc handles the logic for us. (If the first arg is NULL, it just allocates)
  ent->set_pointer(realloc(ent->get_pointer(), new_size));
}

// free `ent->ptr` which was previously allocated by the service.
void alaska::service::free(alaska::Mapping *ent) {
  abort();
  ::free(ent->get_pointer());
  ent->reset();
}

void alaska::service::barrier(void) {
  abort();
  // Do nothing.
}


extern void alaska::service::commit_lock_status(alaska::Mapping *ent, bool locked) {
  abort();
  // do nothing
}
