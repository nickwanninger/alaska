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

    L1_TLB_HIT_RATE,
    L2_TLB_HIT_RATE,
    L1_HTLB_HIT_RATE,
    L2_HTLB_HIT_RATE,
    // UTILIZATION_RATIO,

    COUNT
  };

  class StatisticsManager {
    std::array<float, statistic::COUNT> stats;

   public:
    StatisticsManager(void);
    ~StatisticsManager();

    void incrementStatistic(statistic metric, uint64_t value = 1);

    void compute();
    void reset();
    void dump();
  };
}  // namespace alaska::sim