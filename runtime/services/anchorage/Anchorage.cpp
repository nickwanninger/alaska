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

#include <sys/resource.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <malloc.h>
#include <ck/lock.h>


static ck::mutex anch_lock;

void alaska::service::alloc(alaska::Mapping *ent, size_t size) {
  // static uint64_t last_barrier_time = 0;
  // auto now = alaska_timestamp();
  // if (now - last_barrier_time > 5000 * 1000UL * 1000UL) {
  //   last_barrier_time = now;
  //   alaska::service::barrier();
  // }
  // // alaska::service::barrier();

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
  }

  if (m.ptr != NULL) {
    Block *old_block = anchorage::Block::get(m.ptr);
    auto *old_chunk = anchorage::Chunk::get(m.ptr);
    size_t copy_size = old_block->size();
    if (new_block->size() < copy_size) copy_size = new_block->size();
    memcpy(new_block->data(), old_block->data(), copy_size);

    new_block->set_handle(&m);
    ent->ptr = new_block->data();
    old_chunk->free(old_block);
  } else {
    new_block->set_handle(&m);
    ent->ptr = new_block->data();
  }

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
  auto *blk = anchorage::Block::get(ptr);
  return blk->size();
}



static double get_knob(const char *name, double default_value) {
  if (const char *arg = getenv(name); arg != nullptr) {
    return atof(arg);
  }
  return default_value;
}

static double get_overall_frag(void) {
  anch_lock.lock();

  long rss_bytes = alaska_translate_rss_kb() * 1024;
  long heap_size = 0;

  // Defragment the chunks
  for (auto chunk : anchorage::Chunk::all()) {
    // double frag = chunk->frag();
    heap_size += chunk->active_bytes;
  }
  anch_lock.unlock();

  return rss_bytes / (double)heap_size;
}



static void barrier_control_overhead_target(void) {
	// "target overhead" is the maximum overhead we will permit the defragmenter to impart
	// on the program. This is currently done using a simple control system where we run a
	// barrier then wait for a period of time that would result in the requested overhead.
	// This would be better to do by restricting the runtime of the barrier itself, but that
	// is would be a bit too expensive, as the runtime would have 
  float target_oh = get_knob("ANCH_TARG_OVERHEAD", 0.05);
  // 1.0 is the "best" fragmentation. We will go a bit above that.
	float target_frag = get_knob( "ANCH_TARG_FRAG", 1.1);
  printf("target overhead: %f\n", target_oh);
  printf("target frag:     %f\n", target_frag);

  double ms_to_sleep = 1000;  // 1s out of the gate.
  do {
    usleep(ms_to_sleep * 1000);
    auto start = alaska_timestamp();
    if (get_overall_frag() > target_frag) {
      alaska::service::barrier();
    }
    auto end = alaska_timestamp();
    double ms_spent_defragmenting = (end - start) / 1024.0 / 1024.0;
    ms_to_sleep = ms_spent_defragmenting / target_oh;

    // You can sleep for a minimum of 10ms. This is relatively arbitrary, but ensures progress can
    // be made, and avoids a defrag-storm when there is little work to be done.
    if (ms_to_sleep < 10) {
      ms_to_sleep = 10;
    }

  } while (1);
}



pthread_t anchorage_barrier_thread;
static void *barrier_thread_fn(void *) {
  // You are allowed to spend X% of time working on barriers
  barrier_control_overhead_target();
  return NULL;


  uint64_t barrier_interval_ms = 1000;
  auto boot_time = alaska_timestamp();

  FILE *log = fopen("/tmp/alaska.log", "a");
  double ms_spent_defragmenting = 0;
  // uint64_t thread_start_time = alaska_timestamp();
  while (1) {
    usleep(barrier_interval_ms * 1000);

    anch_lock.lock();

    long rss_bytes = alaska_translate_rss_kb() * 1024;
    long heap_size = 0;

    // Defragment the chunks
    for (auto chunk : anchorage::Chunk::all()) {
      // double frag = chunk->frag();
      heap_size += chunk->active_bytes;
    }
    anch_lock.unlock();

    double overall_fragmentation = rss_bytes / (double)heap_size;
    // double heap_fragmentation = heap_bytes / (double)heap_size;


    if (overall_fragmentation > 1.2) {
      auto start = alaska_timestamp();
      alaska::service::barrier();
      auto end = alaska_timestamp();
      ms_spent_defragmenting += (end - start) / 1024.0 / 1024.0;
    }

    auto time_since_boot = (alaska_timestamp() - boot_time) / 1024.0 / 1024.0;

    fprintf(log, "pid=%d,rss=%lu,frag=%f,defrag_ms=%f,ms=%f,wasted_ratio=%f\n", getpid(), rss_bytes,
        overall_fragmentation, ms_spent_defragmenting, time_since_boot,
        ms_spent_defragmenting / time_since_boot);

    fflush(log);
  }

  fclose(log);

  return NULL;
}


void alaska::service::init(void) {
  anch_lock.init();
  anchorage::allocator_init();
  pthread_create(&anchorage_barrier_thread, NULL, barrier_thread_fn, NULL);
}

// TODO:
void alaska::service::deinit(void) {
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
