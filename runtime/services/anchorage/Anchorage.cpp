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


#include <anchorage/Anchorage.hpp>
#include <anchorage/Chunk.hpp>
#include <anchorage/Block.hpp>
#include <anchorage/Defragmenter.hpp>

#include <alaska.h>
#include <alaska/alaska.hpp>
#include <alaska/service.hpp>
#include <alaska/barrier.hpp>

#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <malloc.h>
#include <ck/lock.h>


static ck::mutex anch_lock;

void alaska::service::alloc(alaska::Mapping *ent, size_t size) {
  static uint64_t last_barrier_time = 0;
  auto now = alaska_timestamp();
  if (now - last_barrier_time > 5000 * 1000UL * 1000UL) {
    last_barrier_time = now;
    alaska::service::barrier();
  }
  // alaska::service::barrier();

  // Grab a lock
  ck::scoped_lock l(anch_lock);

  auto &m = *ent;
  using namespace anchorage;
  Block *new_block = NULL;
  Chunk *new_chunk = NULL;

retry:

  // printf("alloc %zu into %p (there are %zu chunks)\n", size, &m, anchorage::Chunk::all().size());

  // attempt to allocate from each chunk
  for (auto *chunk : anchorage::Chunk::all()) {
    auto blk = chunk->alloc(size);
    // if the chunk could allocate, use the pointer it created
    if (blk) {
      new_chunk = chunk;
      new_block = blk;
			ALASKA_ASSERT(blk < chunk->tos, "An allocated block must be less than the top of stack");
      break;
    }
  }
  (void)new_chunk;

  if (new_block == NULL) {
    size_t required_pages = (size / anchorage::page_size) * 2;
    if (required_pages < anchorage::min_chunk_pages) {
      required_pages = anchorage::min_chunk_pages;
    }

    new anchorage::Chunk(required_pages);  // add a chunk

    goto retry;
    // return alaska::service::alloc(ent, size);
  }

  if (m.ptr != NULL) {
    Block *old_block = anchorage::Block::get(m.ptr);
    auto *old_chunk = anchorage::Chunk::get(m.ptr);
    size_t copy_size = old_block->size();
    if (new_block->size() < copy_size) copy_size = new_block->size();


    // printf("realloc %p -> %p (%zu -> %zu) %zu\n", old_block, new_block, old_block->size(),
    // new_block->size(), copy_size); old_block->dump_content("old before");
    // new_block->dump_content("new before");
    memcpy(new_block->data(), old_block->data(), copy_size);

    // old_block->dump_content("old after");
    // new_block->dump_content("new after");
    // printf("\n");

    new_block->set_handle(&m);
    ent->ptr = new_block->data();
    old_chunk->free(old_block);
  } else {
    new_block->set_handle(&m);
    ent->ptr = new_block->data();
  }


  // printf("[alaska] alloc %p %zu %zu\n", new_block, new_block->size(), size);
  return;
}


void alaska::service::free(alaska::Mapping *ent) {
  ck::scoped_lock l(anch_lock);

  auto *chunk = anchorage::Chunk::get(ent->ptr);
  if (chunk == NULL) {
    fprintf(
        stderr, "[anchorage] attempt to free a pointer not managed by anchorage (%p)\n", ent->ptr);
    return;
  }

  auto *blk = anchorage::Block::get(ent->ptr);
  chunk->free(blk);
  ent->ptr = nullptr;
}

size_t alaska::service::usable_size(void *ptr) {
  // auto *chunk = anchorage::Chunk::get(ptr);
  // // printf("chunk %p\n", chunk);
  // if (chunk == NULL) {
  //   return malloc_usable_size(ptr);
  // }
  // printf("usable %p\n", ptr);
  // alaska_dump_backtrace();
  auto *blk = anchorage::Block::get(ptr);
  // printf("size %p %zu\n", blk, blk->size());
  return blk->size();
}


pthread_t anchorage_barrier_thread;
static uint64_t barrier_interval_ms = 500;
static void *barrier_thread_fn(void *) {
  // uint64_t thread_start_time = alaska_timestamp();
  while (1) {
    usleep(barrier_interval_ms * 1000);
    uint64_t start = alaska_timestamp();
    alaska_barrier();
    // alaska::barrier::begin();
    // printf("SEND\n");
    // sleep(1); // sleep for a second
    // alaska::barrier::end();
    uint64_t end = alaska_timestamp();
    (void)(end - start);
    // printf("Barrier %zu\n", end - start);
  }

  return NULL;
}


void alaska::service::init(void) {
  anch_lock.init();
  anchorage::allocator_init();
  // pthread_create(&anchorage_barrier_thread, NULL, barrier_thread_fn, NULL);
}

void alaska::service::deinit(void) {
  // TODO:
  anchorage::allocator_deinit();
}


void alaska::service::barrier(void) {
  ck::scoped_lock l(anch_lock);

  // Get everyone prepped for a barrier
  alaska::barrier::begin();

  // Defragment the chunks
  for (auto chunk : anchorage::Chunk::all()) {
    long saved = chunk->defragment();
    (void)saved;
    // printf("saved %ld bytes\n", saved);
  }
  // Release everyone from the barrier
  alaska::barrier::end();
}


void alaska::service::commit_lock_status(alaska::Mapping *ent, bool locked) {
  // HACK
  if (ent->ptr == nullptr) return;
  auto *block = anchorage::Block::get(ent->ptr);
  block->mark_locked(locked);
  // printf("Mark %p %d\n", ent, locked);
  // for (auto chunk : anchorage::Chunk::all()) {
  //   // if (chunk->contains(block)) {
  //   chunk->dump(block, "mark");
  //   // }
  //   // long saved = chunk->defragment();
  //   // printf("saved %ld bytes\n", saved);
  // }
}



/// =================================================
const unsigned char anchorage::SizeMap::class_array_[anchorage::SizeMap::kClassArraySize] = {
#include "size_classes.def"
};

const int32_t anchorage::SizeMap::class_to_size_[anchorage::classSizesMax] = {
    16,
    16,
    32,
    48,
    64,
    80,
    96,
    112,
    128,
    160,
    192,
    224,
    256,
    320,
    384,
    448,
    512,
    640,
    768,
    896,
    1024,
    2048,
    4096,
    8192,
    16384,
};
