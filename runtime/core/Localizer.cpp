
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


#include <alaska/Runtime.hpp>
#include <alaska/Localizer.hpp>
#include "alaska/liballoc.h"

namespace alaska {


  Localizer::Localizer(alaska::Configuration &config, alaska::Runtime &rt)
      : rt(rt) {}

  handle_id_t *Localizer::get_hotness_buffer(size_t count) {
    void *x = alaska_internal_malloc(count * sizeof(handle_id_t));
    return (handle_id_t *)x;
  }

  void Localizer::feed_hotness_buffer(size_t count, handle_id_t *handle_ids) {
    printf("========================\n");

    int cols = 0;
    for (size_t i = 0; i < count; i++) {
      printf("%16lx ", handle_ids[i]);
      cols++;
      if (cols >= 8) {
        cols = 0;
        printf("\n");
      }
    }
    if (cols != 0) {
      printf("\n");
    }
    alaska_internal_free(reinterpret_cast<void *>(handle_ids));
  }
}  // namespace alaska
