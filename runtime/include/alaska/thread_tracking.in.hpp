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

#include <alaska/ThreadRegistry.hpp>
#include <alaska/rt.hpp>
#include <alaska/rt/barrier.hpp>
#include <dlfcn.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

// NOTE: this file is a little funky so far as header files go in C++ because it
// provides implementations based on a macro begin defined *before* this file is
// included.
// To enable thread tracking in your alaska runtime, do the following:
//
//    struct my_state {};
//    #define ALASKA_THREAD_TRACK_STATE_T struct my_state
//    #include <alaska/thread_tracking.in.hpp>
//
// This will *define* several functions in the current file. Namely, it will override
// `pthread_create` to enable tracking. Usually, this should just be included wherever
// you implement your barrier logic - and should only be included *ONE TIME*.

#ifndef ALASKA_THREAD_TRACK_STATE_T
#warning Thread state type not defined. defaulting to int
#define ALASKA_THREAD_TRACK_STATE_T int
#endif

// the following is the main interface to this system:
namespace alaska::thread_tracking {
  using StateT = ALASKA_THREAD_TRACK_STATE_T;
  static __thread StateT my_state;

  alaska::ThreadRegistry<StateT *> &threads();

  // Join and leave with the current thread
  void join();
  void leave();
}  // namespace alaska::thread_tracking



// And this is the implementation:
// static auto& threads(void) {
// }

namespace alaska::thread_tracking {
  alaska::ThreadRegistry<StateT *> &threads() {
    static alaska::ThreadRegistry<StateT *> *g_threads;
    if (g_threads == NULL) g_threads = new alaska::ThreadRegistry<StateT *>();
    return *g_threads;
  }


  void join(void) {
#ifdef ALASKA_THREAD_TRACK_INIT
    ALASKA_THREAD_TRACK_INIT;
#endif
    alaska::thread_tracking::threads().join(&alaska::thread_tracking::my_state);
  }

  void leave(void) { alaska::thread_tracking::threads().leave(); }
}  // namespace alaska::thread_tracking





// now, the icky part about wrapping around pthread_create
struct alaska_pthread_trampoline_arg {
  void* arg;
  void* (*start)(void*);
};


static void* alaska_pthread_trampoline(void* varg) {
  void* (*start)(void*);
  auto* arg = (struct alaska_pthread_trampoline_arg*)varg;
  void* thread_arg = arg->arg;
  start = arg->start;
  free(arg);

  alaska::thread_tracking::join();
  void* ret = start(thread_arg);
  alaska::thread_tracking::leave();

  return ret;
}


// Hook into thread creation by overriding the pthread_create function
#undef pthread_create
extern "C" int pthread_create(pthread_t* __restrict thread, const pthread_attr_t* __restrict attr,
    void* (*start)(void*), void* __restrict arg) {
  int rc;
  static int (*real_create)(pthread_t* __restrict thread, const pthread_attr_t* __restrict attr,
      void* (*start)(void*), void* __restrict arg) = NULL;
  if (!real_create) real_create = (decltype(real_create))dlsym(RTLD_NEXT, "pthread_create");

  auto* args = (struct alaska_pthread_trampoline_arg*)calloc(
      1, sizeof(struct alaska_pthread_trampoline_arg));
  args->arg = arg;
  args->start = start;
  rc = real_create(thread, attr, alaska_pthread_trampoline, args);
  return rc;
}

static void __attribute__((constructor(102))) __thread_tracking_init(void) {
  alaska::thread_tracking::join();
}

static void __attribute__((destructor)) __alaska_tracking_deinit(void) {
  alaska::thread_tracking::leave();
}
