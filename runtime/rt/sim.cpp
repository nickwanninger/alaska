#include <ck/lock.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include <alaska/Runtime.hpp>
#include <alaska/sim/HTLB.hpp>
#include "alaska/alaska.hpp"
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

// #define RATE 100'000UL
#define RATE 10'000'000UL
// #define RATE 100'000UL

static long access_count = 0;
static alaska::sim::HTLB *g_htlb = NULL;
static ck::mutex htlb_lock;
static thread_local volatile bool track_on_this_thread = true;


static char sim_name[512];


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


  char log_file[512];
  snprintf(log_file, 256, "%s.csv", sim_name);
  FILE *log = fopen(log_file, "w");

  auto &sm = get_htlb().get_stats();
  sm.dump_csv_header(log);



  alaska::handle_id_t *buf = rt.locality_manager.get_hotness_buffer(TOTAL_ENTRIES);
  memcpy(buf, dump_buf, sizeof(dump_buf));
  rt.locality_manager.feed_hotness_buffer(TOTAL_ENTRIES, dump_buf);


  for (long trial = 0; true; trial++) {
    pthread_mutex_lock(&dump_mutex);
    pthread_cond_wait(&dump_cond, &dump_mutex);

    unsigned long moved_objects = 0;
    unsigned long unmoved_objects = 0;
    unsigned long bytes_in_dump = 0;

#if 0
    rt.with_barrier([&]() {
      for (int i = 0; i < TOTAL_ENTRIES; i++) {
        if (dump_buf[i] == 0) continue;
        auto handle = (void *)(dump_buf[i] << ALASKA_SIZE_BITS);
        auto *m = alaska::Mapping::from_handle_safe(handle);


        bool moved = false;
        if (m == NULL or m->is_free() or m->is_pinned()) {
          moved = false;
        } else {
          void *ptr = m->get_pointer();
          // pages.add((uint64_t)ptr >> 12);
          auto *source_page = rt.heap.pt.get_unaligned(m->get_pointer());
          moved = tc->localize(*m, rt.localization_epoch);
        }

        if (moved) {
          moved_objects++;
          bytes_in_dump += tc->get_size(handle);
        } else {
          unmoved_objects++;
        }
      }


      rt.heap.compact_locality_pages();
      rt.heap.compact_sizedpages();

      // FILE *log = fopen("html/out.html", "w");
      // rt.dump_html(log);
      // fclose(log);


      // {
      //   FILE *out = fopen("html/.out.html", "w");
      //   rt.dump_html(out);
      //   fclose(out);
      //   system("mv html/.out.html html/out.html");
      // }

      // {
      //   FILE *json = fopen("html/.out.json", "w");
      //   rt.heap.dump_json(json);
      //   fclose(json);
      //   system("mv html/.out.json html/out.json");
      // }
    });
#endif

    rt.localization_epoch++;

    sm.compute();
    sm.dump_csv_row(log);

    fflush(log);
    pthread_mutex_unlock(&dump_mutex);
  }


  return NULL;
}

static void __attribute__((constructor)) sim_init(void) {
  printf("Simulation name: ");
  fflush(stdout);
  fgets(sim_name, sizeof(sim_name), stdin);
  size_t len = strlen(sim_name);
  if (len > 0 && sim_name[len - 1] == '\n') {
    sim_name[len - 1] = '\0';
  }

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
    uint32_t offset = maybe_handle & ((1LU << ALASKA_SIZE_BITS) - 1);
    htlb.access(*m, offset);
  } else {
    htlb.access_non_handle((void *)maybe_handle);
  }

  if (access_count > RATE) {
    access_count = 0;

    // dump, and notify the movement thread!
    pthread_mutex_lock(&dump_mutex);


    auto &rt = alaska::Runtime::get();

    alaska::handle_id_t *buf = rt.locality_manager.get_hotness_buffer(TOTAL_ENTRIES);
    htlb.dump_entries(buf);
    // rt.locality_manager.feed_hotness_buffer(TOTAL_ENTRIES, buf);
    rt.locality_manager.feed_hotness_buffer(L1_WAYS * L1_SETS, buf);

    // pthread_cond_signal(&dump_cond);
    pthread_mutex_unlock(&dump_mutex);
  }
}
