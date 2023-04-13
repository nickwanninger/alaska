#pragma once

#include <stdint.h>
#include <alaska/internal.h>
#include <alaska/config.h>

extern uint64_t next_last_access_time;

extern void anchorage_chunk_dump_usage(alaska_mapping_t *);

ALASKA_INLINE void anchorage_on_lock(alaska_mapping_t *m) {
#ifdef ALASKA_ANCHORAGE_PRINT_ACCESS
  anchorage_chunk_dump_usage(m);
#endif
  // m->anchorage.last_access_time = next_last_access_time++;
}

ALASKA_INLINE void anchorage_on_unlock(alaska_mapping_t *m) {
}

#define ALASKA_SERVICE_ON_LOCK(m) anchorage_on_lock(m)
#define ALASKA_SERVICE_ON_UNLOCK(m) anchorage_on_unlock(m)
