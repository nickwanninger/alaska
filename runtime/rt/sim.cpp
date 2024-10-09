#include <ck/lock.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include <alaska/Runtime.hpp>
#include <alaska/sim/HTLB.hpp>
#include "alaska/config.h"
#include "alaska/sim/StatisticsManager.hpp"
#include <sys/time.h>
#include <semaphore.h>


#define L1_ENTS 96
#define L1_SETS 24
#define L1_WAYS (L1_ENTS / L1_SETS)

#define L2_WAYS 16
#define L2_SETS 32
#define TOTAL_ENTRIES (L1_WAYS * L1_SETS + L2_WAYS * L2_SETS)

#define RATE 10'000'000UL

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


static uint64_t time_ms(void) {
  struct timeval tp;
  gettimeofday(&tp, NULL);
  uint64_t ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;
  return ms;
}
static pthread_t sim_background_thread;
static void *sim_background_thread_func(void *) {
  track_on_this_thread = false;

  alaska::wait_for_initialization();
  auto &rt = alaska::Runtime::get();
  auto *tc = rt.new_threadcache();

  FILE *log = fopen("out.csv", "w");
  fprintf(log, "trial,hitrate_baseline\n");



  for (long trial = 0; true; trial++) {
    pthread_mutex_lock(&dump_mutex);
    pthread_cond_wait(&dump_cond, &dump_mutex);

    unsigned long moved_objects = 0;
    unsigned long unmoved_objects = 0;
    unsigned long bytes_in_dump = 0;
    // ck::set<uint64_t> pages;

    // rt.with_barrier([&]() {
    //   rt.heap.compact_locality_pages();
    //   // unsigned long c = rt.heap.compact_sizedpages();
    //   // printf("compacted %lu\n", c);


    //   for (int i = 0; i < TOTAL_ENTRIES; i++) {
    //     if (dump_buf[i] == 0) continue;
    //     auto handle = (void *)(dump_buf[i] << ALASKA_SIZE_BITS);
    //     auto *m = alaska::Mapping::from_handle_safe(handle);


    //     bool moved = false;
    //     if (m == NULL or m->is_free() or m->is_pinned()) {
    //       moved = false;
    //     } else {
    //       void *ptr = m->get_pointer();
    //       // pages.add((uint64_t)ptr >> 12);
    //       auto *source_page = rt.heap.pt.get_unaligned(m->get_pointer());
    //       moved = tc->localize(*m, rt.localization_epoch);
    //     }

    //     if (moved) {
    //       moved_objects++;
    //       bytes_in_dump += tc->get_size(handle);
    //     } else {
    //       unmoved_objects++;
    //     }
    //   }
    // });

    rt.localization_epoch++;

    auto &sm = get_htlb().get_stats();
    sm.compute();
    auto hr = sm.l1_tlb_hr;
    sm.reset();

    fprintf(log, "%lu,%f\n", trial, hr);

    fflush(log);
    pthread_mutex_unlock(&dump_mutex);
  }


  return NULL;
}

static void __attribute__((constructor)) sim_init(void) {
  pthread_create(&sim_background_thread, NULL, sim_background_thread_func, NULL);
}


void alaska_htlb_sim_invalidate(uintptr_t maybe_handle) {
  if (not alaska::is_initialized()) return;
  ck::scoped_lock l(htlb_lock);
  auto m = alaska::Mapping::from_handle_safe((void *)maybe_handle);
  auto &htlb = get_htlb();
  if (m) {
    htlb.invalidate_htlb(*m);
  }
}


void alaska_htlb_sim_track(uintptr_t maybe_handle) {
  if (not alaska::is_initialized()) return;
  ck::scoped_lock l(htlb_lock);
  auto m = alaska::Mapping::from_handle_safe((void *)maybe_handle);
  auto &htlb = get_htlb();
  access_count++;
  if (m) {
    htlb.access(*m);
  } else {
    htlb.access_non_handle((void *)maybe_handle);
  }

  if (access_count > RATE) {
    access_count = 0;

    // dump, and notify the movement thread!
    pthread_mutex_lock(&dump_mutex);

    htlb.dump_entries(dump_buf);
    // htlb.reset();

    pthread_cond_signal(&dump_cond);
    pthread_mutex_unlock(&dump_mutex);
  }
}
