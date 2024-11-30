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
#include <stdio.h>
#include <signal.h>
#include <alaska.h>
// #include <alaska/alaska.hpp>
#include <ctype.h>


#define WEAK __attribute__((weak))

int alaska_wrapped_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
  if (signum == SIGSEGV || signum == SIGILL) {
    printf("Reject sigaction %d\n", signum);
    return -1;
  }
  return sigaction(signum, act, oldact);
}
