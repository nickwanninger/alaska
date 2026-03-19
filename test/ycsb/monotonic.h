#pragma once
#include <stdint.h>
#include <time.h>
typedef uint64_t monotime;
static inline uint64_t getMonotonicUs(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}
static inline void     elapsedStart(monotime *t) { *t = getMonotonicUs(); }
static inline uint64_t elapsedUs(monotime t)     { return getMonotonicUs() - t; }
static inline uint64_t elapsedMs(monotime t)     { return elapsedUs(t) / 1000; }
