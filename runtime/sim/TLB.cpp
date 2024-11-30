

#include <alaska/sim/TLB.hpp>
#include <algorithm>
#include <iomanip>


using namespace alaska::sim;

static uint64_t get_pn(uint64_t vaddr) { return (vaddr & 0x3FFFFFFFFFF000) >> 12; }

void PTEntry::reset(const PTEntry &new_entry) {
  this->vpn = new_entry.vpn;
  this->ppn = new_entry.ppn;
  this->valid = new_entry.valid;
  this->last_used = new_entry.last_used;
}

void PTEntry::dump() {
  printf(
      "[PTEntry] VPN: 0x%lx, PPN: 0x%lx, Valid: %d, Last Used: %ld\n", vpn, ppn, valid, last_used);
}

L2TLB::L2TLB(StatisticsManager &sm, uint32_t num_sets, uint32_t num_ways)
    : sm(sm)
    , num_sets(num_sets)
    , num_ways(num_ways)
    , time(0) {
  sets.resize(num_sets);
  eviction_matrix.resize(num_sets);

  for (auto &set : sets) {
    set.resize(num_ways);
  }

  for (auto &set : eviction_matrix) {
    set.resize(num_ways);
  }
}

uint64_t L2TLB::access(uint64_t vaddr) {
  time++;
  sm.incrementStatistic(L2_TLB_ACCESSES);

  uint64_t vpn = get_pn(vaddr);
  uint64_t offset = vaddr & 0xfff;

  uint64_t way = vpn % num_sets;
  auto entry = std::find_if(sets[way].begin(), sets[way].end(), [&vpn](auto &It) {
    return It.vpn == vpn && It.valid;
  });

  if (entry != sets[way].end()) {
    entry->last_used = time;
    sm.incrementStatistic(L2_TLB_HITS);
    return entry->ppn + offset;
  } else {
    auto oldest_line =
        std::min_element(sets[way].begin(), sets[way].end(), [](auto &ItA, auto &ItB) {
          return ItA.last_used < ItB.last_used;
        });

    auto new_entry = lookup(vpn);
    oldest_line->reset(new_entry);
    return new_entry.ppn + offset;
  }
}

PTEntry L2TLB::pull(uint64_t vaddr, bool is_ht) {
  time++;
  sm.incrementStatistic(L2_TLB_ACCESSES);

  uint64_t vpn = get_pn(vaddr);
  // uint64_t offset = vaddr & 0xfff;

  uint64_t way = vpn % num_sets;
  auto entry = std::find_if(sets[way].begin(), sets[way].end(), [&vpn](auto &It) {
    return It.vpn == vpn && It.valid;
  });

  if (entry != sets[way].end()) {
    entry->last_used = time;
    sm.incrementStatistic(L2_TLB_HITS);
    return *entry;
  } else {
    auto oldest_line =
        std::min_element(sets[way].begin(), sets[way].end(), [](auto &ItA, auto &ItB) {
          return ItA.last_used < ItB.last_used;
        });

    size_t oldest_line_idx = std::distance(sets[way].begin(), oldest_line);
    eviction_matrix[way][oldest_line_idx]++;

    auto new_entry = lookup(vpn);
    oldest_line->reset(new_entry);
    if (is_ht) {
      sm.incrementStatistic(L2_HT_TLB_EVICTIONS);
    }
    sm.incrementStatistic(L2_TLB_EVICTIONS);
    return *oldest_line;
  }
}

PTEntry L2TLB::lookup(uint64_t vpn) {
  PTEntry PTE;

  PTE.vpn = vpn;
  PTE.ppn = vpn;  // BOGUS
  PTE.last_used = 0;
  PTE.valid = true;

  return PTE;
}

void L2TLB::invalidateAll() {
  for (auto &set : sets) {
    for (auto &entry : set) {
      entry.valid = false;
    }
  }
}

L1TLB::L1TLB(StatisticsManager &sm, L2TLB &l2_tlb, uint32_t num_sets, uint32_t num_ways)
    : L2TLB(sm, num_sets, num_ways)
    , l2_tlb(l2_tlb) {
  sets.resize(num_sets);
  eviction_matrix.resize(num_sets);

  for (auto &set : sets) {
    set.resize(num_ways);
  }

  for (auto &set : eviction_matrix) {
    set.resize(num_ways);
  }
}

uint64_t L1TLB::access(uint64_t vaddr, bool is_ht) {
  time++;
  sm.incrementStatistic(L1_TLB_ACCESSES);

  uint64_t vpn = get_pn(vaddr);
  uint64_t offset = vaddr & 0xfff;

  uint64_t way = vpn % num_sets;
  auto entry = std::find_if(sets[way].begin(), sets[way].end(), [&vpn](auto &It) {
    return It.vpn == vpn && It.valid;
  });

  if (entry != sets[way].end()) {
    entry->last_used = time;
    sm.incrementStatistic(L1_TLB_HITS);
    // l2_tlb.PageTable->at(vpn).l1_hits++;
    return entry->ppn + offset;
  } else {
    sm.incrementStatistic(L1_TLB_EVICTIONS);
    auto oldest_line =
        std::min_element(sets[way].begin(), sets[way].end(), [](auto &ItA, auto &ItB) {
          return ItA.last_used < ItB.last_used;
        });

    size_t oldest_line_idx = std::distance(sets[way].begin(), oldest_line);
    eviction_matrix[way][oldest_line_idx]++;

    // printf("[L1TLB] Pulled entry from L2TLB\n");
    // new_entry.dump();
    if (is_ht) {
      auto new_entry = lookup(vaddr, is_ht);
      oldest_line->reset(new_entry);

      sm.incrementStatistic(L1_HT_TLB_EVICTIONS);

      return oldest_line->ppn + offset;
    } else {
      auto new_entry = lookup(vaddr);
      oldest_line->reset(new_entry);
      return oldest_line->ppn + offset;
    }
  }
}

PTEntry L1TLB::lookup(uint64_t vaddr, bool is_ht) { return l2_tlb.pull(vaddr, is_ht); }

void L1TLB::invalidateAll() {
  for (auto &set : sets) {
    for (auto &entry : set) {
      entry.valid = false;
    }
  }
}
