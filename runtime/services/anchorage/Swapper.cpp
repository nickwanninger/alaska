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


#include <anchorage/Swapper.hpp>
#include <alaska/alaska.hpp>
#include <alaska/service.hpp>

#include <ck/pair.h>
#include <ck/map.h>
#include <ck/lock.h>

void alaska::service::swap_in(alaska::Mapping *m) {
  anchorage::swap_in(*m);
}


void anchorage::swap_in(alaska::Mapping &m) {
}


void anchorage::swap_out(alaska::Mapping &m) {
}
