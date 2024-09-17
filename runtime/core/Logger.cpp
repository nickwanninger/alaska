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
#include <unistd.h>
#include <string.h>

static const char *level_strings[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};

static const char *level_colors[] = {
    "\x1b[94m", "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[35m"};


static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static int log_level = LOG_ERROR;
static bool enable_colors = true;

// If the log level is coming from ENV, lock it so it cannot change.
static bool log_level_locked = false;

static void __attribute__((constructor(102))) alaska_logger_init(void) {
  if (isatty(2)) {
    enable_colors = true;
  } else {
    enable_colors = false;
  }



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



#define BUFFER_SIZE 1024

static int log_vemitf(const char *format, va_list args) {
  char buffer[BUFFER_SIZE];
  int printed_length;
  // Format the string into the buffer using snprintf
  printed_length = vsnprintf(buffer, BUFFER_SIZE, format, args);

  // Clean up variadic arguments
  va_end(args);

  // Check for truncation
  if (printed_length < 0 || printed_length >= BUFFER_SIZE) {
    // Handle buffer overflow or other snprintf errors if needed
    return -1;
  }

  // Write the formatted string to stdout
  if (write(STDOUT_FILENO, buffer, printed_length) != printed_length) {
    // Handle write error
    return -1;
  }

  return printed_length;  // Return the number of characters written
}

static int log_emitf(const char *format, ...) {
  va_list args;

  // Initialize variadic arguments
  va_start(args, format);

  return log_vemitf(format, args);
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

      if (enable_colors) {
        log_emitf("%s %s%-5s\x1b[0m \x1b[90m%s:%d\x1b[0m | ", buf, level_colors[level],
            level_strings[level], file, line);
      } else {
        log_emitf("%s %-5s %s:%d | ", buf, level_strings[level], file, line);
      }

      log_vemitf(fmt, args);
      log_emitf("\n");

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


  int printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_vemitf(fmt, args);
    return 0;
  }
}  // namespace alaska
