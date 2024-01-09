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
#include <anchorage/Block.hpp>
#include <anchorage/Chunk.hpp>

#include <alaska.h>
#include <alaska/alaska.hpp>
#include <alaska/barrier.hpp>
#include <alaska/service.hpp>
#include <alaska/table.hpp>

#include <ck/lock.h>
#include <malloc.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>

static ck::mutex anch_lock;

void alaska::service::alloc(alaska::Mapping *ent, size_t size) {
  void *ptr = ent->get_pointer();
  // Grab a lock
  anch_lock.lock();

  auto &m = *ent;
  using namespace anchorage;
  Block *new_block = NULL;

  // Allocate from the to_space
  auto new_chunk = anchorage::Chunk::to_space;
  auto blk = new_chunk->alloc(size);
  new_block = blk;
  ALASKA_ASSERT(blk < new_chunk->tos, "An allocated block must be less than the top of stack");
  (void)new_chunk;

  // If we couldn't find anything, trigger a barrier.
  // TODO: mark *why* we want to barrier somewhere...
  //       We should prioritize compaction
  if (new_block == NULL) {
    anch_lock.unlock();
    alaska::service::barrier();
    alaska::service::alloc(ent, size);
    return;
  }

  if (ptr != NULL) {
    Block *old_block = anchorage::Block::get(ptr);
    auto *old_chunk = anchorage::Chunk::get(ptr);
    size_t copy_size = old_block->size();
    if (new_block->size() < copy_size) copy_size = new_block->size();
    memcpy(new_block->data(), old_block->data(), copy_size);

    new_block->set_handle(&m);
    ent->set_pointer(new_block->data());
    old_chunk->free(old_block);
  } else {
    new_block->set_handle(&m);
    ent->set_pointer(new_block->data());
  }

  anch_lock.unlock();
  return;
}

void alaska::service::free(alaska::Mapping *ent) {
  anch_lock.lock();

  auto *chunk = anchorage::Chunk::get(ent->get_pointer());

  if (chunk) {
    auto *blk = anchorage::Block::get(ent->get_pointer());
    chunk->free(blk);
    ent->set_pointer(nullptr);
  } else {
    fprintf(stderr, "[anchorage] attempt to free a pointer not managed by anchorage (%p)\n",
        ent->get_pointer());
  }


  anch_lock.unlock();
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

  heap_size += anchorage::Chunk::to_space->active_bytes;
  heap_size += anchorage::Chunk::from_space->active_bytes;

  anch_lock.unlock();

  return rss_bytes / (double)heap_size;
}

double anchorage::get_heap_frag_locked(void) {
  long rss_bytes = 0;
  long heap_size = 0;

  rss_bytes += anchorage::Chunk::to_space->span();
  rss_bytes += anchorage::Chunk::from_space->span();

  heap_size += anchorage::Chunk::to_space->memory_used_including_overheads();
  heap_size += anchorage::Chunk::from_space->memory_used_including_overheads();

  return rss_bytes / (double)heap_size;
}

double anchorage::get_heap_frag(void) {
  anch_lock.lock();
  double val = get_heap_frag_locked();
  anch_lock.unlock();
  return val;
}

void pad_barrier_control_overhead_target(void) {
  float target_oh_lb = get_knob("ANCH_TARG_OVERHEAD_LB", 0.001);  // .01%  min overhead
  float target_oh_ub = get_knob("ANCH_TARG_OVERHEAD_UB", 0.10);   //  10% max overhead
  float target_frag_lb = get_knob("ANCH_TARG_FRAG_LB", 1.4);
  float target_frag_ub = get_knob("ANCH_TARG_FRAG_UB", 2.0);
  float target_frag_improve_min = get_knob("ANCH_TARG_FRAG_IMPROVE_MIN", 0.05);
  float aimd_add = get_knob("ANCH_AIMD_ADD", 1);
  float aimd_mul = get_knob("ANCH_AIMD_MUL", 0.5);

  // set initial state to be "right on target"
  float this_frag_start;
  float this_frag_end;
  float this_oh_start;
  float this_cost;
  float cost_since_last_pass = 0;

  int did_pass = 0;
  int hit_lb = 0;
  int hit_min = 0;
  float last_pass_start = 0;

  // control variable
  double ms_to_sleep = 100;  // 1s out of the gate.

  printf("target overhead: [%f,%f]\n", target_oh_lb, target_oh_ub);
  printf("target frag:     [%f,%f]\n", target_frag_lb, target_frag_ub);

  do {
    printf("sleeping for %f ms\n", ms_to_sleep);
    usleep(ms_to_sleep * 1000);
    auto start = alaska_timestamp();
    this_frag_start = anchorage::get_heap_frag();
    this_oh_start = cost_since_last_pass / (start - last_pass_start);

    // long rss_bytes = alaska_translate_rss_kb() * 1024;
    // printf("current frag = %f, rss = %fmb\n", this_frag_start, rss_bytes /
    // 1024.0 / 1024.0);

    // do a pass only if we are past the upper fragmentation limit, and our
    // overhead is too low
    if ((this_frag_start > target_frag_ub) && (this_oh_start < target_oh_lb)) {
      printf(
          "current frag of %f exceeds high water mark and have time - doing "
          "pass\n",
          this_frag_start);
      // alaska::service::barrier(target_frag_lb);
      alaska::service::barrier();
      // estimate fragmentation at end of pass
      // does not need to be this slow
      this_frag_end = anchorage::get_heap_frag();
      did_pass = 1;
      hit_lb = this_frag_end < target_frag_lb;
      hit_min = (this_frag_start - this_frag_end) > target_frag_improve_min;
      last_pass_start = start;
      cost_since_last_pass = 0;  // reset here, will be computed later
                                 // long rss_bytes = alaska_translate_rss_kb() * 1024;
      printf("pass complete - resulting frag is %f (%s) %fMB\n", this_frag_end,
          hit_lb ? "hit" : "MISS", alaska_translate_rss_kb() / 1024.0);
    } else {
      this_frag_end = this_frag_start;
      did_pass = 0;
      hit_lb = 1;
      hit_min = 1;
    }
    auto end = alaska_timestamp();
    this_cost = (end - start);
    cost_since_last_pass += this_cost;

    // now update the control variable based on the regime we are in
    if (did_pass) {
      if (hit_lb) {
        // normal behavior, do additive increase (slow down) to reduce overhead
        ms_to_sleep += aimd_add;
      } else {
        // we missed hitting the fragmentation target
        // it may be impossible for us to reach the defrag target, though
        if (hit_min) {
          // we did improve the situation a bit, so let's try more frequently
          // multiplicative decrease (speed up)
          ms_to_sleep *= aimd_mul;
        } else {
          // we did not improve things, keep time same
          // ms_to_sleep = ms_to_sleep;
        }
      }
    } else {
      // we did not do a pass, which means we ran too soon,
      // so additive increase (slow down)
      ms_to_sleep += aimd_add;
    }

    if (ms_to_sleep < 10) {
      ms_to_sleep = 10;
    }

  } while (1);
}

static void barrier_control_overhead_target(void) {
  enum State {
    COMPACTING,  // We are actively moving objects.
    WAITING,     // We are waiting until frag is high enough to begin defrag
  };

  State state = WAITING;
  long total_moved_this_cycle = 0;
  double ms_spent_in_this_cycle = 0;
  unsigned long movement_start = 0;

  // This adjusts according to the control system.
  long tokens_to_spend = 10'000'000;
  long max_tokens = 100'000'000;

  // "target overhead" is the maximum overhead we will permit the defragmenter
  // to impart on the program. This is currently done using a simple control
  // system where we run a barrier then wait for a period of time that would
  // result in the requested overhead. This would be better to do by restricting
  // the runtime of the barrier itself, but that is would be a bit too
  // expensive, as the runtime would have
  float target_oh = get_knob("ANCH_TARG_OVERHEAD", 0.05);
  // How much of the heap to move in a single run, when we are in the moving
  // state.
  float aggressiveness = get_knob("ANCH_AGGRO", 0.10);

  float frag_lb = get_knob("FRAG_LB", 1.05);  // if frag < lb, stop compacting.
  float frag_ub = get_knob("FRAG_UB", 1.4);   // if frag > ub, start compacting

  if (frag_lb > frag_ub) {
    frag_ub = frag_lb + 0.20;
  }
  printf("target overhead: %f\n", target_oh);
  printf("frag bounds:    [%f, %f]\n", frag_lb, frag_ub);

  double ms_to_sleep = 1000;
  do {
    // ms_to_sleep = 400;
    usleep(ms_to_sleep * 1000);

    auto start = alaska_timestamp();
    float frag_before = anchorage::get_heap_frag();

    auto to_use = anchorage::Chunk::to_space->memory_used_including_overheads();
    auto from_use = anchorage::Chunk::from_space->memory_used_including_overheads();

    if (state == WAITING) {
      // If the fragmentation has gotten "out of hand", switch to the compacting
      // state.
      if (frag_before > frag_ub) {
        state = COMPACTING;
        total_moved_this_cycle = 0;
        ms_spent_in_this_cycle = 0;
        tokens_to_spend = aggressiveness * to_use;
        // if (tokens_to_spend > max_tokens) {
        //   tokens_to_spend = max_tokens;
        // }
        movement_start = start;
        printf("Starting movement! Tokens=%lu\n", tokens_to_spend);

        // When entering the COMPACTING state, swap the spaces.
        // This ensures that allocations are made to the *to space* while, and
        // objects are moved from the *from_space*
        anchorage::Chunk::swap_spaces();
      }
    }

    if (state == COMPACTING) {
      ck::scoped_lock l(anch_lock);
      // Get everyone prepped for a barrier
      alaska::barrier::begin();
      anchorage::CompactionConfig config;
      config.available_tokens = tokens_to_spend;  // how many bytes can you defrag?
      // Perform compaction. Transfer objects from the `from_space` to the `to_space`
      printf("Compacting! Tokens = %5.2fMB\n", tokens_to_spend / 1024.0 / 1024.0);
      auto moved =
          anchorage::Chunk::from_space->perform_compaction(*anchorage::Chunk::to_space, config);
      total_moved_this_cycle += moved;
      ms_spent_in_this_cycle += (alaska_timestamp() - start) / 1000.0 / 1000.0;

      float frag_after = anchorage::get_heap_frag_locked();

      alaska::barrier::end();
      if (moved == 0 or frag_after < frag_lb or moved < tokens_to_spend) {
        state = WAITING;
        printf("Stopping. Ran out of things to move. Swapping.\n");
      } else if (frag_after < frag_lb) {
        printf("Stopping. hit lower bound. Not swapping\n");
        state = WAITING;
      }
    }

    if (state == COMPACTING) {
      auto end = alaska_timestamp();
      double ms_spent_defragmenting = (end - start) / 1000.0 / 1000.0;
      // If we are actively defragmenting. Respect the overhead request of the
      // user.
      ms_to_sleep = ms_spent_defragmenting / target_oh;
    } else if (state == WAITING) {
      ms_to_sleep = 500;  // come back later!
    }
    // You can sleep for a minimum of 10ms. This is relatively arbitrary, but
    // ensures progress can be made, and avoids a defrag-storm when there is
    // little work to be done.
    if (ms_to_sleep < 10) {
      ms_to_sleep = 10;
    }

    fprintf(stderr, "[ctrl frag=%2.4f %ld %ld (rss=%lu)] ", frag_before, to_use / 1024 / 1024,
        from_use / 1024 / 1024, alaska_translate_rss_kb() / 1024);
    switch (state) {
      case WAITING:
        fprintf(stderr, "Waiting...\n", frag_before);
        break;
      case COMPACTING: {
        float ach =
            (ms_spent_in_this_cycle * 1000.0 * 1000.0) / (alaska_timestamp() - movement_start);
        float ms_this_cycle = (alaska_timestamp() - start) / 1000.0 / 1000.0;
        fprintf(stderr, "Done (%5.2fMB in %fms total, %fms this run (ach:%.1f%%))\n",
            total_moved_this_cycle / 1024.0 / 1024.0, ms_spent_in_this_cycle, ms_this_cycle,
            ach * 100.0);
        break;
      }
    }

  } while (1);
}

extern void alaska_dump_thread_states(void);
static void barrier_simple_time(void) {
  // while(1) {
  //   alaska_dump_thread_states();
  //   usleep(1000);
  // }
  long moved = 0;
  (void)moved;
  usleep(500 * 1000);
  while (1) {
    usleep(10 * 1000);
    // continue;
    //
    anch_lock.lock();
    // Get everyone prepped for a barrier
    alaska::barrier::begin();
    // anchorage::CompactionConfig config;
    // moved = anchorage::Chunk::to_space->perform_compaction(*anchorage::Chunk::from_space,
    // config); anchorage::Chunk::swap_spaces(); auto end = alaska_timestamp(); printf("%lu moved
    // barrier took %lf ms\n", moved, (end - start) / 1000.0 / 1000.0); (void)(end - start);

    alaska::barrier::end();
    anch_lock.unlock();
    // Swap the spaces and switch to waiting.
    // anchorage::Chunk::swap_spaces();
  }
}



static void stress_workload(void) {
  // The interval that the stress test should work in.
  // Defaults to once a second.
  long interval = get_knob("STRESS_INTERVAL", 1000);
  // How much of the heap should be moved each time?
  // Defaults to 1mb
  long tokens = get_knob("STRESS_TOKENS", 1024 * 1024);

  if (interval < 10) {
    interval = 10;
    fprintf(stderr, "[anchorage] Cannot stress lower than 10ms\n");
  }



  usleep(500 * 1000);
  while (true) {
    usleep(interval * 1000);

    auto start =alaska_timestamp();
    anch_lock.lock();
    alaska::barrier::begin();
    anchorage::CompactionConfig config;

    config.available_tokens = tokens;

    if (anchorage::Chunk::from_space->memory_used_including_overheads() < tokens) {
      anchorage::Chunk::swap_spaces();
    }

    auto moved =
        anchorage::Chunk::from_space->perform_compaction(*anchorage::Chunk::to_space, config);

    if (moved == 0) {
      anchorage::Chunk::swap_spaces();
    }

    alaska::barrier::end();
    anch_lock.unlock();

    auto end = alaska_timestamp();
    // printf("Moved %d in %lf\n", moved, (end - start) / (1000.0 * 1000.0));
  }
}



static pthread_t anchorage_barrier_thread;
static void *barrier_thread_fn(void *) {
  // Mark this thread as escaped. (TODO: is this even needed anymore?)
  alaska_thread_state.escaped = 1;
  pthread_setname_np(pthread_self(), "anchorage");
  char *mode = getenv("ANCH_MODE");

  if (mode == NULL || strcmp(mode, "defrag") == 0) {
    // pad_barrier_control_overhead_target();
    barrier_control_overhead_target();
    // barrier_simple_time();
  } else if (strcmp(mode, "stress") == 0) {
    stress_workload();
  } else if (strcmp(mode, "disable") == 0) {
    // Nothing!
  } else {
    fprintf(stderr, "[anchorage] unknown mode, '%s'\n", mode);
    abort();
  }
  return NULL;
}

void alaska::service::init(void) {
  anch_lock.init();

  anchorage::allocator_init();

  if (getenv("ANCH_NO_DEFRAG") == NULL) {
    pthread_create(&anchorage_barrier_thread, NULL, barrier_thread_fn, NULL);
  }
}

void alaska::service::deinit(void) {
  anchorage::allocator_deinit();
}

void alaska::service::barrier(void) {
  // Don't do anything. Anchorage fires it's own barriers.
}

void alaska::service::commit_lock_status(alaska::Mapping *ent, bool locked) {
  // HACK
  if (ent->get_pointer() == nullptr) return;

  auto *block = anchorage::Block::get(ent->get_pointer());

  if (anchorage::Chunk::to_space->contains(block) ||
      anchorage::Chunk::from_space->contains(block)) {
    block->mark_locked(locked);
  }
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
