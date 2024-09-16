#include <ck/lock.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include <alaska/Runtime.hpp>
#include <alaska/sim/HTLB.hpp>
#include "alaska/config.h"
#include <semaphore.h>


#define L1_ENTS 96
#define L1_SETS 24
#define L1_WAYS (L1_ENTS / L1_SETS)

#define L2_WAYS 16
#define L2_SETS 32
#define TOTAL_ENTRIES (L1_WAYS * L1_SETS + L2_WAYS * L2_SETS)

#define RATE 10'000

static long access_count = 0;
static alaska::sim::HTLB *g_htlb = NULL;
static ck::mutex htlb_lock;
static thread_local volatile bool track_on_this_thread = true;


static pthread_mutex_t dump_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t dump_cond = PTHREAD_COND_INITIALIZER;
static uint64_t dump_buf[TOTAL_ENTRIES * 2];

extern alaska::ThreadCache *get_tc(void);

static alaska::sim::HTLB &get_htlb(void) {
  if (g_htlb == NULL) {
    g_htlb = new alaska::sim::HTLB(L1_SETS, L1_WAYS, L2_SETS, L2_WAYS);
  }

  return *g_htlb;
}

static pthread_t sim_background_thread;
static void *sim_background_thread_func(void *) {
  track_on_this_thread = false;

  alaska::wait_for_initialization();

  while (true) {
    pthread_mutex_lock(&dump_mutex);
    pthread_cond_wait(&dump_cond, &dump_mutex);


    // // Got a dump!
    auto &rt = alaska::Runtime::get();

    rt.with_barrier([&]() {
      unsigned long c = rt.heap.jumble();
      printf("jumbled %lu objects\n", c);
      // rt.heap.compact_sizedpages();
      return;


      unsigned long moved_objects = 0;
      for (int i = 0; i < TOTAL_ENTRIES; i++) {
        if (dump_buf[i] == 0) continue;
        uint64_t val = dump_buf[i] << ALASKA_SIZE_BITS;
        if (get_tc()->localize((void *)val, rt.localization_epoch)) {
          moved_objects++;
        }
      }
      auto sm = get_htlb().get_stats();
      sm.compute();
      // sm.dump();
      rt.localization_epoch++;
      printf("Objects moved: %lu\n", moved_objects);
      printf("TLB Hitrates: %f%% %f%%\n", sm.l1_tlb_hr, sm.l2_tlb_hr);
    });
    pthread_mutex_unlock(&dump_mutex);
  }


  return NULL;
}

static void __attribute__((constructor)) sim_init(void) {
  pthread_create(&sim_background_thread, NULL, sim_background_thread_func, NULL);
}



void alaska_htlb_sim_track(uintptr_t maybe_handle) {
  if (not alaska::is_initialized()) return;
  ck::scoped_lock l(htlb_lock);
  auto m = alaska::Mapping::from_handle_safe((void *)maybe_handle);
  auto &htlb = get_htlb();
  if (m) {
    htlb.access(*m);
    access_count++;

    if (access_count > RATE) {
      access_count = 0;

      // dump, and notify the movement thread!
      pthread_mutex_lock(&dump_mutex);

      htlb.dump_entries(dump_buf);
      // htlb.reset();

      pthread_cond_signal(&dump_cond);
      pthread_mutex_unlock(&dump_mutex);
    }
  } else {
    htlb.access_non_handle((void *)maybe_handle);
  }
}
