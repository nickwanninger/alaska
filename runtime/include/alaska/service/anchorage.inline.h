#pragma once

#include <stdint.h>
#include <alaska/internal.h>

extern uint64_t next_last_access_time;

extern void alaska_classify_track(uint8_t object_class);
ALASKA_INLINE void anchorage_on_lock(alaska_mapping_t *m) {
  // m->anchorage.last_access_time = next_last_access_time++;
  // atomic_inc(m->anchorage.locks, 1);
  m->anchorage.locks++;
}

ALASKA_INLINE void anchorage_on_unlock(alaska_mapping_t *m) {
  // atomic_dec(m->anchorage.locks, 1);
  // atomic_inc(m->anchorage.unlocks, 1);
  m->anchorage.locks--;
}

#define ALASKA_SERVICE_ON_LOCK(m) anchorage_on_lock(m)
#define ALASKA_SERVICE_ON_UNLOCK(m) anchorage_on_unlock(m)
