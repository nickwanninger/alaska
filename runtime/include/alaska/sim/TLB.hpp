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
#pragma once


#include <cstdint>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <alaska/sim/StatisticsManager.hpp>


namespace alaska::sim {

  struct PTEntry {
    uint64_t vpn;
    uint64_t ppn;
    bool valid;
    uint64_t l1_hits, l2_hits;

    uint64_t last_used;

    PTEntry()
        : vpn(0)
        , ppn(0)
        , valid(false)
        , last_used(0)
        , l1_hits(0)
        , l2_hits(0) {}
    PTEntry(uint64_t vpn, uint64_t ppn, bool valid, uint64_t last_used)
        : vpn(vpn)
        , ppn(ppn)
        , valid(valid)
        , last_used(last_used) {}

    void reset(const PTEntry &new_entry);

    void dump();
  };

  class L2TLB {
   protected:
    StatisticsManager &sm;
    uint32_t num_sets, num_ways;
    uint64_t time;
    std::vector<std::vector<PTEntry>> sets;

   public:
    L2TLB(StatisticsManager &sm, uint32_t num_sets, uint32_t num_ways);
    ~L2TLB() = default;

    uint64_t access(uint64_t vaddr);
    PTEntry lookup(uint64_t vpn);

    PTEntry pull(uint64_t vpn, bool is_ht = false);

    std::vector<std::vector<uint64_t>> eviction_matrix;

    virtual void invalidateAll();
  };

  class L1TLB : public L2TLB {
    L2TLB &l2_tlb;

   public:
    L1TLB(StatisticsManager &sm, L2TLB &l2_tlb, uint32_t num_sets, uint32_t num_ways);
    ~L1TLB() = default;

    uint64_t access(uint64_t vaddr, bool is_ht = false);
    PTEntry lookup(uint64_t vpn, bool is_ht = false);

    void invalidateAll() override;
  };

}  // namespace alaska::sim
