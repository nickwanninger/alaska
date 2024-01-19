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

#include <alaska/service.hpp>
#include <ck/map.h>
#include <ck/string.h>

static alaska::Service *default_service = NULL;


static ck::map<ck::string, alaska::Service *> services;

void alaska::register_service(alaska::Service &s, bool isDefault) {
  printf("Register service %s\n", s.name());
  if (isDefault || default_service == NULL) {
    default_service = &s;
  }
  // Simply add it to the list of services.
  services[s.name()] = &s;
}




alaska::Service &alaska::get_service(const char *name) {
  auto s = services[name];
}



alaska::Service &alaska::get_default_service(void) {
  ALASKA_ASSERT(default_service != NULL, "There must be a default service registered.");
  return *default_service;
}
