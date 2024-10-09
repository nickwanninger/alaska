#pragma once

#include <unordered_map>
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <string>
#include <unordered_map>

namespace alaska::sim {
  enum statistic {
    L1_TLB_HITS,
    L1_TLB_ACCESSES,
    L1_TLB_EVICTIONS,
    L1_HT_TLB_EVICTIONS,
    L2_TLB_HITS,
    L2_TLB_ACCESSES,
    L2_TLB_EVICTIONS,
    L2_HT_TLB_EVICTIONS,
    L1_HTLB_HITS,
    L1_HTLB_ACCESSES,
    L2_HTLB_HITS,
    L2_HTLB_ACCESSES,
    HT_TLB_LOOKUPS,

    // EDGES,
    // EPOCH_MAX_VPAGE,
    // EPOCH_MIN_VPAGE,
    // EPOCH_PAGES,
    // EPOCH_COMPACTION_HANDLE_SPACE,
    // EPOCH_COMPACTION_MEM_SPACE,
    // EPOCH_MEM_MOVED,
    // UTILIZATION_RATIO,

    COUNT
  };

  class StatisticsManager {
    std::array<uint64_t, statistic::COUNT> stats;

   public:
    float l1_tlb_hr = 0;
    float l2_tlb_hr = 0;
    float l1_htlb_hr = 0;
    float l2_htlb_hr = 0;

    StatisticsManager(void);
    ~StatisticsManager();

    void incrementStatistic(statistic metric, uint64_t value = 1);
    uint64_t getStatistic(statistic s);

    void compute();
    void reset();
    void dump();
  };
}  // namespace alaska::sim
