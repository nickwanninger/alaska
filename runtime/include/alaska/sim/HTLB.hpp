#pragma once

#include <alaska/sim/TLB.hpp>
#include <alaska/sim/StatisticsManager.hpp>
#include <alaska/ThreadCache.hpp>
#include <cstdint>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <iostream>

namespace alaska::sim {

  enum class CACHE_INCLUSION_POLICY { INCLUSIVE, NOT_INCLUSIVE, EXCLUSIVE };

  struct HTLBEntry {
    uint64_t hid;
    uint64_t addr;
    bool phys;
    bool valid;

    uint64_t last_used;
    bool recently_moved;

    HTLBEntry()
        : hid(0)
        , addr(0)
        , phys(false)
        , valid(false) {}

    void reset(const HTLBEntry &new_entry);
    void dump();
  };

  struct HTEntry {
    uint64_t hid;
    uint64_t addr;
    uint64_t size;
    bool recently_moved;
    bool valid;

    HTEntry() = default;
    HTEntry(uint64_t hid, uint64_t addr, uint64_t size, bool recently_moved, bool valid)
        : hid(hid)
        , addr(addr)
        , size(size)
        , recently_moved(recently_moved)
        , valid(valid) {}

    void dump();
  };

  struct HTLBResp {
    uint64_t addr;
    bool phys;

    HTLBResp(uint64_t addr, bool phys)
        : addr(addr)
        , phys(phys) {}
  };

  class L2HTLB {
   protected:
    StatisticsManager &sm;
    L1TLB *tlb;
    uint32_t num_sets, num_ways;
    uint64_t time;
    CACHE_INCLUSION_POLICY policy;

    std::vector<std::vector<HTLBEntry>> sets;

   public:
    L2HTLB(StatisticsManager &sm, int num_sets, int num_ways, L1TLB *tlb,
        CACHE_INCLUSION_POLICY policy = CACHE_INCLUSION_POLICY::EXCLUSIVE);
    ~L2HTLB() = default;

    virtual HTLBEntry lookup(alaska::Mapping &m);
    HTLBEntry pull(alaska::Mapping &m);

    bool insert(const HTLBEntry &entry);
    virtual void invalidateAll();

    std::vector<HTLBEntry> getAllEntriesSorted();

    virtual std::vector<uint64_t> getHandles();
  };

  class L1HTLB : public L2HTLB {
    L2HTLB &l2_htlb;

   public:
    L1HTLB(StatisticsManager &sm, L2HTLB &l2_htlb, int num_sets, int num_ways,
        CACHE_INCLUSION_POLICY policy = CACHE_INCLUSION_POLICY::EXCLUSIVE);
    ~L1HTLB() = default;

    HTLBResp access(alaska::Mapping &m);
    HTLBEntry lookup(alaska::Mapping &m) override;

    void invalidateAll() override;
    std::vector<uint64_t> getHandles() override;
  };




  class HTLB final {
    StatisticsManager sm;
    L2TLB l2_tlb;
    L1TLB l1_tlb;

    L2HTLB l2_htlb;
    L1HTLB l1_htlb;

    std::vector<uint64_t> full_access_trace_;

   public:
    HTLB(int l1_num_sets, int l1_num_ways, int l2_num_sets, int l2_num_ways,
        CACHE_INCLUSION_POLICY policy = CACHE_INCLUSION_POLICY::EXCLUSIVE);
    ~HTLB();

    static HTLB *get();

    uint64_t last_accessed = 0;
    uint64_t access_ind = 0;

    void dump_entries(uint64_t *dest);
    void reset(void) {
      access_ind = 0;
      last_accessed = 0;
      l1_htlb.invalidateAll();
      l2_htlb.invalidateAll();
      l1_tlb.invalidateAll();
      l2_tlb.invalidateAll();
      full_access_trace_.clear();
    }

    inline HTLBResp access(alaska::Mapping &m) {
      auto h = m.encode();
      full_access_trace_.push_back(h);
      // if (last_accessed != 0 and last_accessed != h) {
      //   printf("  x%lx -> x%lx [label=%lu, color=grey, constraint=false]\n", last_accessed, h,
      //   access_ind++);
      // }
      // last_accessed = h;
      return l1_htlb.access(m);
    }


    // return the access trace
    std::vector<uint64_t> get_access_trace() { return full_access_trace_; }

    // just a thread cache which is attached to the HTLB so that we can use it
    // to allocate handles when simulating.
    alaska::ThreadCache *thread_cache = nullptr;
  };
}  // namespace alaska::sim
