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
#define STAT(S) S,
#include "./stats.inc"
#undef STAT

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

    void dump_csv_header(FILE *out);
    void dump_csv_row(FILE *out);

    void compute();
    void reset();
    void dump();
  };
}  // namespace alaska::sim
