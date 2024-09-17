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
#include <alaska/rt/barrier.hpp>
#include <pthread.h>
#include <stdio.h>
#include <signal.h>



static alaska::Runtime *the_runtime = nullptr;


struct CompilerRuntimeBarrierManager : public alaska::BarrierManager {
  ~CompilerRuntimeBarrierManager() override = default;
  void begin(void) override { alaska::barrier::begin(); }
  void end(void) override { alaska::barrier::end(); }
};

static CompilerRuntimeBarrierManager the_barrier_manager;

extern "C" void alaska_dump(void) { the_runtime->dump(stderr); }


static pthread_t barrier_thread;
static void *barrier_thread_func(void *) {
  return NULL;
  while (1) {
    usleep(50 * 1000);

    alaska::Runtime::get().with_barrier([]() {
      long swapped = alaska::Runtime::get().heap.jumble();
      printf("Swapped %ld\n", swapped);
    });
    // alaska::barrier::begin();
    // printf("Barrier.\n");
    // alaska::barrier::end();
  }

  return NULL;
}

void __attribute__((constructor(102))) alaska_init(void) {
  alaska::barrier::add_self_thread();
  // Allocate the runtime simply by creating a new instance of it. Everywhere
  // we use it, we will use alaska::Runtime::get() to get the singleton instance.
  the_runtime = new alaska::Runtime();
  // Attach the runtime's barrier manager
  the_runtime->barrier_manager = &the_barrier_manager;

  // pthread_create(&barrier_thread, NULL, barrier_thread_func, NULL);
}

void __attribute__((destructor)) alaska_deinit(void) {
  // pthread_kill(barrier_thread, SIGKILL);
  // pthread_join(barrier_thread, NULL);

  // Note: we don't currently care about deinitializing the runtime for now, since the application
  // is about to die and all it's memory is going to be cleaned up.
  delete the_runtime;

  alaska::barrier::remove_self_thread();
}
