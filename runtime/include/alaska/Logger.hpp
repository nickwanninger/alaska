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


#pragma once

#include <stdio.h>

enum { LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL };


namespace alaska {
  void log(int level, const char *file, int line, const char *fmt, ...);


  void set_log_level(int level);

  int printf(const char *format, ...);
};  // namespace alaska



#ifdef ALASKA_ENABLE_LOGGING
#define log_trace(...) alaska::log(LOG_TRACE, __FILE_NAME__, __LINE__, __VA_ARGS__)
#define log_debug(...) alaska::log(LOG_DEBUG, __FILE_NAME__, __LINE__, __VA_ARGS__)
#define log_info(...) alaska::log(LOG_INFO, __FILE_NAME__, __LINE__, __VA_ARGS__)
#define log_warn(...) alaska::log(LOG_WARN, __FILE_NAME__, __LINE__, __VA_ARGS__)
#define log_error(...) alaska::log(LOG_ERROR, __FILE_NAME__, __LINE__, __VA_ARGS__)
#define log_fatal(...) alaska::log(LOG_FATAL, __FILE_NAME__, __LINE__, __VA_ARGS__)
#else
#define log_trace(...)
#define log_debug(...)
#define log_info(...)
#define log_warn(...)
#define log_error(...)
#define log_fatal(...)
#endif
