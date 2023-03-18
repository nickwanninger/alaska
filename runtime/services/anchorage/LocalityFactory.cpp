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

#include <algorithm>
#include <anchorage/Block.hpp>
#include <anchorage/Chunk.hpp>
#include <anchorage/LocalityFactory.hpp>
#include <alaska/internal.h>
#include <limits.h>
#include <pthread.h>
#include <string.h>
#include <vector>

anchorage::LocalityFactory::LocalityFactory(void *root) {
  // go perform a mark of (conservatively) reachable allocations
  mark_reachable((uint64_t)root);
}

void anchorage::LocalityFactory::mark_reachable(uint64_t possible_handle) {
  alaska::Mapping *m = alaska_lookup((void *)possible_handle);

  // It wasn't a handle, don't mark.
  if (m == NULL) return;

  // Was it well formed (allocated?)
  if (m < alaska_table_begin() || m >= alaska_table_end() || m->size == (uint32_t)-1) {
    return;
  }

  // Don't mark again.
  if (reachable.count(m) != 0) return;
  // "Mark"
  // TODO: do this less shit.
  reachable.insert(m);

  auto data = (uint64_t *)m->ptr;
  for (uint64_t i = 0; i < m->size; i += sizeof(uint64_t)) {
    mark_reachable(data[i]);
  }
}


void anchorage::LocalityFactory::run(void) {
  std::vector<alaska::Mapping *> mappings(reachable.begin(), reachable.end());
  // anchorage::barrier(true);
  // for (auto *m : mappings) {
  //   printf("Reachable: %p\n", m);
  // }
  //
  // printf("sort:\n");
  std::sort(mappings.begin(), mappings.end(), [&](alaska::Mapping *left, alaska::Mapping *right) {
    return left->anchorage.last_access_time < right->anchorage.last_access_time;
  });

  for (auto *m : mappings) {
    anchorage::alloc(*m, m->size);
  }
  anchorage::barrier(true);
}
