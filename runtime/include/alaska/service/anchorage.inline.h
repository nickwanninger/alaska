#pragma once

#include <stdint.h>
#include <alaska/internal.h>

extern uint64_t next_last_access_time;

extern void alaska_classify_track(uint8_t object_class);
extern void anchorage_chunk_dump_usage(alaska_mapping_t *);

ALASKA_INLINE void anchorage_on_lock(alaska_mapping_t *m) {
  // anchorage_chunk_dump_usage(m);
  // m->anchorage.last_access_time = next_last_access_time++;
}

ALASKA_INLINE void anchorage_on_unlock(alaska_mapping_t *m) {
}

#define ALASKA_SERVICE_ON_LOCK(m) anchorage_on_lock(m)
#define ALASKA_SERVICE_ON_UNLOCK(m) anchorage_on_unlock(m)
