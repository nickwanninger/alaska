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


#include <alaska/utils.h>
#include <alaska/Logger.hpp>
#include <stdio.h>
#include <string.h>
#include <alaska/Runtime.hpp>

void alaska_dump_backtrace() {
  FILE *stream = fopen("/proc/self/maps", "r");

  fprintf(stderr, "Memory Map:\n");
  char line[1024];
  while (fgets(line, sizeof(line), stream) != NULL) {
    fwrite(line, strlen(line), 1, stderr);
  }

  fclose(stream);


  auto *rt = alaska::Runtime::get_ptr();
  if (rt) {
    fprintf(stderr, "Heap dump:\n");
    rt->heap.dump_json(stderr);
  }

}
