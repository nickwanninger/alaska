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



#include <alaska/Logger.hpp>
#include <pthread.h>
#include <stdarg.h>
#include <stdlib.h>


static const char *level_strings[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};

static const char *level_colors[] = {
    "\x1b[94m", "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[35m"};


static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static int log_level = LOG_ERROR;

// If the log level is coming from ENV, lock it so it cannot change.
static bool log_level_locked = false;

static void __attribute__((constructor(102))) alaska_init(void) {
  const char *env = getenv("ALASKA_LOG_LEVEL");
  if (env != NULL) {
    if (env[0] == 'T') log_level = LOG_TRACE;
    if (env[0] == 'D') log_level = LOG_DEBUG;
    if (env[0] == 'I') log_level = LOG_INFO;
    if (env[0] == 'W') log_level = LOG_WARN;
    if (env[0] == 'E') log_level = LOG_ERROR;
    if (env[0] == 'F') log_level = LOG_FATAL;

    log_level_locked = true;
  }
}




namespace alaska {
  void log(int level, const char *file, int line, const char *fmt, ...) {
    if (level >= log_level) {
      va_list args;
      va_start(args, fmt);

      // Grab the lock
      pthread_mutex_lock(&log_mutex);


      time_t t = time(NULL);
      auto time = localtime(&t);

      char buf[16];
      buf[strftime(buf, sizeof(buf), "%H:%M:%S", time)] = '\0';
      fprintf(stderr, "%s %s%-5s\x1b[0m \x1b[90m%s:%d:\x1b[0m ", buf, level_colors[level],
          level_strings[level], file, line);
      vfprintf(stderr, fmt, args);
      fprintf(stderr, "\n");

      // release the lock
      pthread_mutex_unlock(&log_mutex);
      va_end(args);
    }
  }


  void set_log_level(int level) {
    if (!log_level_locked) {
      log_level = level;
    }
  }
}  // namespace alaska