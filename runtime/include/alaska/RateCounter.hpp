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

#include <stdio.h>
#include <alaska.h>
#include <alaska/utils.h>

namespace alaska {

  class TimeCache {
    uint64_t m_now;

   public:
    inline TimeCache(void) { m_now = alaska_timestamp(); }
    inline auto now(void) const { return m_now; }
  };

  class RateCounter {
    uint64_t last_nanoseconds = 0;
    uint64_t last_value = 0;
    uint64_t value = 0;
    float last_rate = 0.0;

   public:
    inline RateCounter() {
      value = 0;
      digest();
    }


    // Read the value, and return the rate (per second) since the last digest.
    // This function can take a time cache, which can be used to
    // record the exact same time for many RateCounter readings. This
    // is useful because reading time from the kernel could be expensive
    inline float digest(const TimeCache &tc) {
      auto now = alaska_timestamp();
      auto ns_passed = now - last_nanoseconds;
      float seconds_passed = ns_passed / 1000.0 / 1000.0 / 1000.0;
      uint64_t change = value - last_value;

      // Only update the rate every 100ms
      if (seconds_passed < 0.1) return last_rate;

      last_nanoseconds = now;
      last_value = value;
      last_rate = change / seconds_passed;

      return last_rate;
    }

    inline float digest(void) {
      alaska::TimeCache tc;
      return digest(tc);
    }


    // Track that an event happened
    inline void track(int incr = 1) { value += incr; }
    inline void track_atomic(int incr = 0) { atomic_inc(this->value, incr); }
    // Return the total count tracked by the counter
    inline uint64_t read(void) const { return value; }
  };




  struct AllocationRate {
    float allocation_bytes_per_second;
    float free_bytes_per_second;

    float tendancy(void) {
      float a = allocation_bytes_per_second;
      float f = free_bytes_per_second;
      return (a - f) / (a + f + 10e-6);
    }

    float pressure(void) {
      float a = allocation_bytes_per_second;
      float f = free_bytes_per_second;
      return a / (f + 10e-6);
    }

    float delta(void) {
      return allocation_bytes_per_second - free_bytes_per_second;
    }
  };
  class AllocationRateCounter {
    RateCounter allocations;
    RateCounter frees;

   public:
    void alloc(size_t size) { allocations.track(size); }
    void free(size_t size) { frees.track_atomic(size); }
    AllocationRate digest(TimeCache &tc) {
      return AllocationRate{allocations.digest(tc), frees.digest(tc)};
    }
  };
};  // namespace alaska
