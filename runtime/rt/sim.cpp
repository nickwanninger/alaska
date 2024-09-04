#include <ck/lock.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include <alaska/sim/HTLB.hpp>


#define L1_ENTS 96
#define L1_SETS 24
#define L1_WAYS (L1_ENTS / L1_SETS)

#define L2_WAYS 16
#define L2_SETS 32
#define TOTAL_ENTRIES (L1_WAYS * L1_SETS + L2_WAYS * L2_SETS)


#define RATE 100'000'000

static long access_count = 0;
static alaska::sim::HTLB *htlb;
static ck::mutex htlb_lock;

static alaska::sim::HTLB &get_htlb() {
  if (unlikely(htlb == nullptr)) {
    htlb = new alaska::sim::HTLB(L1_SETS, L1_WAYS, L2_SETS, L2_WAYS);
  }

  return *htlb;
}

void alaska_htlb_sim_track(uintptr_t maybe_handle) {
  ck::scoped_lock l(htlb_lock);
  auto m = alaska::Mapping::from_handle_safe((void *)maybe_handle);
  if (m) {
    auto &htlb = get_htlb();
    htlb.access(*m);
    access_count++;

    if (access_count > RATE) {
      access_count = 0;

      // uint64_t *buf = (uint64_t *)calloc(TOTAL_ENTRIES, sizeof(uint64_t));
      // htlb.dump_entries(buf);
      // free(buf);

      auto sm = htlb.get_stats();
      sm.compute();
      sm.dump();
      // Reset the htlb for this run
      htlb.reset();
    }
  } else {
    auto &htlb = get_htlb();
    htlb.access_non_handle((void*)maybe_handle);
  }
}
