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


// This file contains the initialization and deinitialization functions for the
// alaska::Runtime instance, as well as some other bookkeeping logic.

#include <alaska/rt.hpp>
#include <alaska/Runtime.hpp>
#include <alaska/alaska.hpp>
#include <stdio.h>

static alaska::Runtime *the_runtime = nullptr;

extern "C" void alaska_dump(void) {
  the_runtime->heap.dump(stderr);
}

void __attribute__((constructor(102))) alaska_init(void) {
  // Allocate the runtime simply by creating a new instance of it. Everywhere
  // we use it, we will use alaska::Runtime::get() to get the singleton instance.
  the_runtime = alaska::make_object<alaska::Runtime>();
}

void __attribute__((destructor)) alaska_deinit(void) {
  // Note: we don't currently care about deinitializing the runtime for now, since the application
  // is about to die and all it's memory is going to be cleaned up.
  alaska::delete_object(the_runtime);
}
