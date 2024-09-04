#include <alaska/sim/StatisticsManager.hpp>
#include <chrono>
#include <stdexcept>
#include <unordered_map>

#include <alaska/sim/StatisticsManager.hpp>

using namespace alaska::sim;


StatisticsManager::StatisticsManager()
    : stats() {}

StatisticsManager::~StatisticsManager() {}

void StatisticsManager::incrementStatistic(statistic metric, uint64_t value) {
  stats[metric] += value;
}


//
void StatisticsManager::compute() {
  l1_tlb_hr = 100 * (double)stats[L1_TLB_HITS] / stats[L1_TLB_ACCESSES];
  l2_tlb_hr = 100 * (double)stats[L2_TLB_HITS] / stats[L2_TLB_ACCESSES];
  l1_htlb_hr = 100 * (double)stats[L1_HTLB_HITS] / stats[L1_HTLB_ACCESSES];
  l2_htlb_hr = 100 * (double)stats[L2_HTLB_HITS] / stats[L2_HTLB_ACCESSES];
}

void StatisticsManager::reset() {
  for (auto &stat : stats) {
    stat = 0;
  }

  l1_tlb_hr = 0;
  l2_tlb_hr = 0;
  l1_htlb_hr = 0;
  l2_htlb_hr = 0;
}

void StatisticsManager::dump() {
#define D(x) printf("%20s: %lu\n", #x, stats[x])
  printf("==== Statistics ====\n");


  printf("TLB:\n");
  D(L1_TLB_HITS);
  D(L1_TLB_ACCESSES);
  D(L1_TLB_EVICTIONS);
  D(L1_HT_TLB_EVICTIONS);
  printf("\n");


  D(L2_TLB_HITS);
  D(L2_TLB_ACCESSES);
  D(L2_TLB_EVICTIONS);
  D(L2_HT_TLB_EVICTIONS);
  printf("\n");

  printf("HTLB:\n");
  D(L1_HTLB_HITS);
  D(L1_HTLB_ACCESSES);
  D(L2_HTLB_HITS);
  D(L2_HTLB_ACCESSES);
  D(HT_TLB_LOOKUPS);
  printf("\n");
  // D(EDGES);
  // D(EPOCH_MAX_VPAGE);
  // D(EPOCH_MIN_VPAGE);
  // D(EPOCH_PAGES);
  // D(EPOCH_COMPACTION_HANDLE_SPACE);
  // D(EPOCH_COMPACTION_MEM_SPACE);
  // D(EPOCH_MEM_MOVED);
  printf("Hit Rates:\n");
#undef D
#define D(x, n) printf("%20s: %f%%\n", n, x)
  D(l1_tlb_hr, "L1 TLB");
  D(l2_tlb_hr, "L2 TLB");
  D(l1_htlb_hr, "L1 HTLB");
  D(l2_htlb_hr, "L2 HTLB");
  // D(L1_TLB_HIT_RATE);
  // D(L2_TLB_HIT_RATE);
  // D(L1_HTLB_HIT_RATE);
  // D(L2_HTLB_HIT_RATE);
  printf("\n");
#undef D

  // for (auto metric = 0; metric < statistic::COUNT; metric++) {
  //   stats_file << magic_enum::enum_name((statistic)metric) << "," << epoch << "," << replay <<
  //   ","; if (metric >= L1_TLB_HIT_RATE) {
  //     stats_file << std::setprecision(4) << stats[metric] << "\n";
  //   } else {
  //     stats_file << (size_t)stats[metric] << "\n";
  //   }
  // }
}
