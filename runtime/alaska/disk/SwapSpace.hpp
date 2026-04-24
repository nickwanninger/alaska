/*
 * This file is part of the Alaska Handle-Based Memory Management System
 *
 * Copyright (c) 2026, Nick Wanninger <ncw@u.northwestern.edu>
 * Copyright (c) 2026, The Constellation Project
 * All rights reserved.
 *
 */

#pragma once

#include <alaska/disk/Structure.hpp>
#include "alaska/alaska.hpp"
#include <ck/lock.h>
#include <ck/map.h>

namespace alaska {
  struct Runtime;
  class ThreadCache;
  struct ObjectHeader;
}

namespace alaska::disk {

  class SwapSpace final : public Structure, public InternalHeapAllocated {
   public:
    SwapSpace(alaska::Runtime &runtime, BufferPool &pool, const char *name = "swap");

    bool swap_out(alaska::Mapping *mapping);
    bool handle_fault(alaska::Mapping *mapping, alaska::ThreadCache *actor);
    bool discard(alaska::Mapping *mapping);

    void debug_dump(FILE *out);
   private:
    struct RootHeader {
      uint32_t cookie;
      uint32_t head_used_bytes;
      uint64_t head_page_id;
      uint64_t swapped_out_count;
      uint64_t fault_in_count;
      uint64_t discard_count;
    };

    static constexpr uint32_t COOKIE = 'SWAP';

    alaska::Runtime &runtime;
    ck::mutex lock;
    ck::map<uint64_t, uint32_t> page_used_bytes;
    ck::map<uint64_t, uint32_t> live_records;

    RateCounter swap_out_byte_rate;
    RateCounter fault_in_byte_rate;

    RootHeader load_root();
    void store_root(const RootHeader &root);

    uint64_t head_record_offset(const RootHeader &root) const;
    bool page_is_empty(uint64_t page_id) const;
    void note_live_record(uint64_t page_id);
    void note_dead_record(uint64_t page_id);
    void reclaim_page_if_empty(uint64_t page_id, RootHeader &root);
    bool validate_record(size_t used_bytes, size_t offset, const alaska::ObjectHeader &header) const;
    bool release_resident_object(alaska::Mapping *mapping, void *ptr);

  };

}  // namespace alaska::disk
