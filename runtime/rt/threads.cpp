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


#include <alaska/rt.hpp>
#include <alaska/barrier.hpp>
#include <dlfcn.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

// This file wraps pthread_create to record threads that come online.

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


  alaska::barrier::add_self_thread();
  void* ret = start(thread_arg);
  alaska::barrier::remove_self_thread();

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
