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


#include <alaska/alaska.hpp>
#include <alaska/sim/HTLB.hpp>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iomanip>
#include <vector>

using namespace alaska::sim;

static uint64_t get_pn(uint64_t vaddr) { return (vaddr & 0x3FFFFFFFFFF000) >> 12; }

void HTLBEntry::reset(const HTLBEntry &new_entry) {
  this->hid = new_entry.hid;
  this->addr = new_entry.addr;
  this->phys = new_entry.phys;
  this->valid = new_entry.valid;
  this->last_used = new_entry.last_used;
  this->recently_moved = new_entry.recently_moved;
}

void HTLBEntry::dump() {
  printf(
      "[HTLBEntry] HID: 0x%lx, Addr: 0x%lx, Phys: %d, Valid: %d, Last Used: "
      "%ld, Recently Moved: %d\n",
      hid, addr, phys, valid, last_used, recently_moved);
}

void HTEntry::dump() {
  printf(
      "[HTEntry] HID: 0x%lx, Addr: 0x%lx, Size: 0x%lx, Recently Moved: %d, "
      "Valid: %d\n",
      hid, addr, size, recently_moved, valid);
}

L2HTLB::L2HTLB(
    StatisticsManager &sm, int num_sets, int num_ways, L1TLB *tlb, CACHE_INCLUSION_POLICY policy)
    : sm(sm)
    , tlb(tlb)
    , num_ways(num_ways)
    , num_sets(num_sets)
    , time(0)
    , policy(policy) {
  assert(policy != CACHE_INCLUSION_POLICY::INCLUSIVE && "Not implemented yet");
  sets.resize(num_sets);

  for (auto &set : sets) {
    set.resize(num_ways);
  }
}

HTLBEntry L2HTLB::pull(alaska::Mapping &m) {
  time++;
  sm.incrementStatistic(L2_HTLB_ACCESSES);

  uint64_t hid = m.handle_id();
  // uint64_t offset = haddr & 0xffffffff;

  uint32_t way = hid % num_sets;
  auto entry = std::find_if(sets[way].begin(), sets[way].end(), [&hid](auto &It) {
    return It.hid == hid && It.valid;
  });

  if (entry != sets[way].end()) {
    sm.incrementStatistic(L2_HTLB_HITS);
    if (policy == CACHE_INCLUSION_POLICY::EXCLUSIVE) {
      auto found_entry = *entry;
      entry->valid = false;
      found_entry.last_used = time;
      return found_entry;
    } else {
      entry->last_used = time;
      return *entry;
    }
  } else {
    auto new_entry = lookup(m);
    if (policy != CACHE_INCLUSION_POLICY::EXCLUSIVE) {
      auto oldest_line =
          std::min_element(sets[way].begin(), sets[way].end(), [](auto &ItA, auto &ItB) {
            return ItA.last_used < ItB.last_used;
          });

      oldest_line->reset(new_entry);
      return *oldest_line;
    } else {
      return new_entry;
    }
  }
}

HTLBEntry L2HTLB::lookup(alaska::Mapping &m) {
  HTLBEntry HTE;

  // simulated TLB access for lookup in handle table
  sm.incrementStatistic(HT_TLB_LOOKUPS);
  tlb->access((uint64_t)&m, true);



  HTE.hid = m.handle_id();
  HTE.addr = (uint64_t)m.get_pointer();
  HTE.phys = false;
  HTE.valid = not m.is_free();
  HTE.last_used = 0;
  HTE.recently_moved = false;  // TODO

  if (HTE.phys) {
    // Physical optimization:
    // the first time we lookup an entry, we also access
    // the tlb at the location of the handle's data.
    tlb->access(HTE.addr, false);
  }

  return HTE;
}

bool L2HTLB::insert(const HTLBEntry &new_entry) {
  auto hid = new_entry.hid;
  uint32_t way = hid % num_sets;

  auto entry = std::find_if(sets[way].begin(), sets[way].end(), [&hid](auto &It) {
    return It.hid == hid && It.valid;
  });

  if (entry != sets[way].end()) {
    entry->dump();
    assert("Entry already exists in L2HTLB" && false);
    return false;
  } else {
    auto oldest_line =
        std::min_element(sets[way].begin(), sets[way].end(), [](auto &ItA, auto &ItB) {
          return ItA.last_used < ItB.last_used;
        });

    oldest_line->reset(new_entry);
    return true;
  }
}

void L2HTLB::invalidateAll() {
  for (auto &set : sets) {
    for (auto &entry : set) {
      entry.valid = false;
      entry.hid = 0;
      entry.last_used = 0;
    }
  }
}

void L2HTLB::invalidate(alaska::Mapping &m) {
  for (auto &set : sets) {
    for (auto &entry : set) {
      if (entry.hid == m.handle_id()) {
        entry.valid = false;
        entry.hid = 0;
        entry.last_used = 0;
      }
    }
  }
}

std::vector<uint64_t> L2HTLB::getHandles() {
  std::vector<uint64_t> handles;
  for (auto &set : sets) {
    for (auto &entry : set) {
      if (entry.valid) {
        handles.push_back(entry.hid);
      }
    }
  }

  return handles;
}



std::vector<HTLBEntry> L2HTLB::getAllEntriesSorted() {
  std::vector<HTLBEntry> entries;
  for (auto &set : sets) {
    std::vector<HTLBEntry> entries_in_set;
    for (auto &entry : set) {
      entries_in_set.push_back(entry);
    }
    std::stable_sort(entries_in_set.begin(), entries_in_set.end(), [](auto &ItA, auto &ItB) {
      return ItA.last_used < ItB.last_used;
    });

    for (auto &entry : entries_in_set) {
      entries.push_back(entry);
    }
  }


  return entries;
}


std::vector<HTLBEntry> L2HTLB::getAllEntries() {
  std::vector<HTLBEntry> entries;
  for (auto &set : sets) {
    for (auto &entry : set) {
      entries.push_back(entry);
    }
  }
  return entries;
}


L1HTLB::L1HTLB(StatisticsManager &sm, L2HTLB &l2_htlb, int num_sets, int num_ways,
    CACHE_INCLUSION_POLICY policy)
    : l2_htlb(l2_htlb)
    , L2HTLB(sm, num_sets, num_ways, nullptr, policy) {
  sets.resize(num_sets);
  for (auto &set : sets) {
    set.resize(num_ways);
  }
}

HTLBResp L1HTLB::access(alaska::Mapping &m, uint32_t offset) {
  time++;
  sm.incrementStatistic(L1_HTLB_ACCESSES);

  auto hid = m.handle_id();
  uint32_t way = hid % num_sets;
  auto entry = std::find_if(sets[way].begin(), sets[way].end(), [&](auto &It) {
    return It.hid == hid && It.valid;
  });

  HTLBResp resp(0, false);

  if (entry != sets[way].end()) {
    entry->last_used = time;
    sm.incrementStatistic(L1_HTLB_HITS);
    resp = {entry->addr, entry->phys};
  } else {
    auto oldest_line =
        std::min_element(sets[way].begin(), sets[way].end(), [](auto &ItA, auto &ItB) {
          return ItA.last_used < ItB.last_used;
        });

    auto new_entry = l2_htlb.pull(m);
    new_entry.last_used = time;
    oldest_line->reset(new_entry);

    if (policy == CACHE_INCLUSION_POLICY::EXCLUSIVE && oldest_line->valid) {
      l2_htlb.insert(*oldest_line);
    }

    resp = {oldest_line->addr, oldest_line->phys};
  }


  auto resp_page = resp.addr >> 12;
  auto acc_page = (resp.addr + offset) >> 12;

  if (not resp.phys or resp_page != acc_page) {
    l2_htlb.tlb->access(resp.addr + offset, false);
  } else {
  }

  return resp;
}

HTLBEntry L1HTLB::lookup(alaska::Mapping &m) { return l2_htlb.pull(m); }

void L1HTLB::invalidateAll() {
  for (auto &set : sets) {
    for (auto &entry : set) {
      entry.valid = false;
    }
  }
}

std::vector<uint64_t> L1HTLB::getHandles() {
  std::vector<uint64_t> handles;
  for (auto &set : sets) {
    for (auto &entry : set) {
      if (entry.valid) {
        handles.push_back(entry.hid);
      }
    }
  }

  return handles;
}



static HTLB *g_htlb = nullptr;

HTLB *HTLB::get() {
  ALASKA_SANITY(g_htlb != nullptr, "HTLB not created");
  return g_htlb;
}

HTLB::HTLB(int l1_num_sets, int l1_num_ways, int l2_num_sets, int l2_num_ways,
    CACHE_INCLUSION_POLICY policy)
    : l2_tlb(sm, 128, 12)
    , l1_tlb(sm, l2_tlb, 16, 4)
    , l2_htlb(sm, l2_num_sets, l2_num_ways, &l1_tlb, policy)
    , l1_htlb(sm, l2_htlb, l1_num_sets, l1_num_ways, policy) {
  ALASKA_SANITY(g_htlb == nullptr, "Only one HTLB can be created");
  g_htlb = this;
}

HTLB::~HTLB() {
  // sm.compute();
  // sm.dump();
  g_htlb = nullptr;
}


void HTLB::dump_entries(uint64_t *dest) {
  auto l1e = l1_htlb.getAllEntriesSorted();
  int makeup_count = 0;

  for (auto &entry : l1e) {
    if (entry.valid) {
      *dest++ = entry.hid;
    } else {
      makeup_count++;
    }
  }

  auto l2e = l2_htlb.getAllEntriesSorted();
  for (auto &entry : l2e) {
    if (entry.valid) {
      *dest++ = entry.hid;
    } else {
      makeup_count++;
    }
  }

  for (int i = 0; i < makeup_count; i++) {
    *dest++ = 0;
  }
}


void HTLB::dump_debug() {
  auto print_ents = [](auto ents) {
    fprintf(stderr, "[");
    for (auto &entry : ents) {
      auto val = entry.valid ? entry.hid : 0;
      fprintf(stderr, " %12lx", val);
    }
    fprintf(stderr, " ]");
  };

  print_ents(l1_htlb.getAllEntries());
  print_ents(l2_htlb.getAllEntries());
  fprintf(stderr, "\n");
}


#undef MASK
