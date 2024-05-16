/*
 * This file is part of the Alaska Handle-Based Memory Management System
 *
 * Copyright (c) 2024, Nick Wanninger <ncw@u.northwestern.edu>
 * Copyright (c) 2024, The Constellation Project
 * All rights reserved.
 *
 * This is free software.  You are permitted to use, redistribute,
 * and modify it as specified in the file "LICENSE".
 */

#include <alaska/alaska.hpp>
#include <alaska/table.hpp>
#include <pthread.h>
#include <string.h>
#include <sys/mman.h>
#include <ck/vec.h>



// allocate a table entry
alaska::Mapping *alaska::table::get(void) { return nullptr; }
void alaska::table::put(alaska::Mapping *ent) {}
void alaska::table::init() {}
void alaska::table::deinit() {}
alaska::Mapping *alaska::table::begin(void) { return nullptr; }
alaska::Mapping *alaska::table::end(void) { return nullptr; }
