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


#include <alaska/util/utils.h>
#include <alaska/util/Logger.hpp>
#include <stdio.h>
#include <string.h>
#include <alaska/core/Runtime.hpp>


#include <execinfo.h>

void alaska_dump_backtrace() {
  // FILE *stream = fopen("/proc/self/maps", "r");

  // fprintf(stderr, "Memory Map:\n");
  // char line[1024];
  // while (fgets(line, sizeof(line), stream) != NULL) {
  //   fwrite(line, strlen(line), 1, stderr);
  // }

  // fclose(stream);

  void *buffer[50];                   // Buffer to store backtrace addresses
  int nptrs = backtrace(buffer, 50);  // Get backtrace addresses

  // Convert addresses to function names
  char **symbols = backtrace_symbols(buffer, nptrs);
  if (symbols == NULL) {
    perror("backtrace_symbols");
    exit(EXIT_FAILURE);
  }

  printf("Backtrace:\n");
  for (int i = 0; i < nptrs; i++) {
    printf("%s\n", symbols[i]);
  }

  free(symbols);  // Free memory allocated by backtrace_symbols
}
