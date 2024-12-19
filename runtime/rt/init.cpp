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


// This file contains the initialization and deinitialization functions for the
// alaska::Runtime instance, as well as some other bookkeeping logic.

#include <alaska/rt.hpp>
#include <alaska/Runtime.hpp>
#include <alaska/alaska.hpp>
#include <alaska/rt/barrier.hpp>
#include "alaska/ObjectReference.hpp"
#include <pthread.h>
#include <stdio.h>
#include <signal.h>
#include <ck/queue.h>
#include <alaska/RateCounter.hpp>

static alaska::Runtime *the_runtime = nullptr;


struct CompilerRuntimeBarrierManager : public alaska::BarrierManager {
  ~CompilerRuntimeBarrierManager() override = default;
  bool begin(void) override { return alaska::barrier::begin(); }
  void end(void) override { alaska::barrier::end(); }
};

static CompilerRuntimeBarrierManager the_barrier_manager;

extern "C" void alaska_dump(void) { the_runtime->dump(stderr); }


static pthread_t barrier_thread;
static void *barrier_thread_func(void *) {
  bool in_marking_state = true;

  FILE *log_file = fopen("cold.csv", "w");
  // fprintf(log_file, "cold_objects\n");
  while (1) {
    auto &rt = alaska::Runtime::get();
    usleep(50 * 1000);

    // continue;
    long already_invalid = 0;
    long total_handles = 0;
    long newly_marked = 0;

    float cold_perc = 0;
    // Linear Congruential Generator parameters
    unsigned int seed = 123456789;  // You can set this to any initial value

    rt.with_barrier([&]() {
      // printf("\033[2J\033[H");
      // rt.handle_table.dump(stdout);
      rt.heap.dump(stdout);
      return;


      auto rng = [&]() -> unsigned long {
        seed = (1103515245 * seed + 12345) & 0x7fffffff;
        return seed;
      };

      // Generate a random number between min and max (inclusive)
      auto random_range = [&](int min, int max) -> unsigned long {
        return min + (rng() % (max - min + 1));
      };

      auto should_mark = [&]() -> bool {
        // return in_marking_state;
        // return in_marking_state and (random_range(0, 2) == 1);
        return random_range(0, 10) == 1;
      };


      auto mark_in_slab = [&](alaska::HandleSlab *slab) {
        for (auto *v : slab->allocator) {
          auto *m = (alaska::Mapping *)v;
          if (m->is_invalid()) {
            already_invalid++;
          }
          total_handles++;

          if (not m->is_pinned() and should_mark()) {
            newly_marked++;
            m->set_invalid();
          }
        }
      };

      auto &slabs = rt.handle_table.get_slabs();
      // printf("slabs = %d\n", slabs.size());

      auto start = alaska_timestamp();
      if (slabs.size() != 0) {
        auto *slab = slabs[random_range(0, slabs.size() - 1)];
        mark_in_slab(slab);
      }
      // for (auto *slab : slabs)
      //   mark_in_slab(slab);

      auto end = alaska_timestamp();

      auto duration = (end - start) / 1000.0 / 1000.0;



      cold_perc = ((float)already_invalid / (float)total_handles);
      if (cold_perc > 0.7) {
        in_marking_state = false;
      }

      if (cold_perc < 0.1) {
        in_marking_state = true;
      }

      fprintf(log_file, "%f\n", cold_perc * 100.0);
      fflush(log_file);
      // printf(
      //     "%d | cold objects: %12.6f%% | %8lu inv | %8lu obj | %8zu faults | %8lu added |
      //     %12fms\n", in_marking_state, 100.0 * cold_perc, already_invalid, total_handles,
      //     rt.handle_faults, newly_marked, duration);
      // printf("Compacting\n");
      // alaska::Runtime::get().heap.compact_sizedpages();
    });
  }

  return NULL;
}

void __attribute__((constructor(102))) alaska_init(void) {
  // Allocate the runtime simply by creating a new instance of it. Everywhere
  // we use it, we will use alaska::Runtime::get() to get the singleton instance.
  the_runtime = new alaska::Runtime();
  // Attach the runtime's barrier manager
  the_runtime->barrier_manager = &the_barrier_manager;
  pthread_create(&barrier_thread, NULL, barrier_thread_func, NULL);
}

void __attribute__((destructor)) alaska_deinit(void) {}
