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
#include <deque>


static alaska::Mapping *safe_lookup(void *possible_handle) {
  alaska::Mapping *m = alaska_lookup((void *)possible_handle);

  // It wasn't a handle, don't mark.
  if (m == NULL) return NULL;

  // Was it well formed (allocated?)
  if (m < alaska_table_begin() || m >= alaska_table_end() || m->size == (uint32_t)-1) {
    return NULL;
  }

  return m;
}


anchorage::LocalityFactory::LocalityFactory(void *root) {
  auto *m = safe_lookup(root);
  if (m) traverse(m);
}


void anchorage::LocalityFactory::traverse(alaska::Mapping *m) {
  // printf("traverse %p %zu\n", m, m->size);
  auto data = (uint8_t *)m->ptr;
  for (uint64_t i = 0; i < m->size; i += sizeof(uint64_t)) {
    uint64_t *cur = (uint64_t *)((uint8_t *)data + i);
    auto *m = safe_lookup((void *)*cur);
    if (m == NULL || reachable.count(m) != 0) return;
    auto *blk = anchorage::Block::get(m->ptr);
    if (blk->is_locked()) {
      return;
    }
    order.push_back(m);
    reachable.insert(m);
    traverse(m);
  }
}

void anchorage::LocalityFactory::run(void) {
  // Simply move all the allocations we found to the end of the heap.
  // TODO: perform this with a to-space Chunk. We need to be smarter about moving things between chunks.
  for (auto *m : order) {
    auto *chunk = anchorage::Chunk::get(m->ptr);
    auto *blk = anchorage::Block::get(m->ptr);
    chunk->dump(blk, "manloc");
    anchorage::alloc(*m, m->size);
  }
}
