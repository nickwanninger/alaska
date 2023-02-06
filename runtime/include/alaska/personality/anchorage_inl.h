#pragma once

#include <stdint.h>
#include <alaska/internal.h>

extern void alaska_classify_track(uint8_t object_class);
ALASKA_INLINE void anchorage_on_lock(alaska_mapping_t *m) {
	atomic_inc(m->anchorage.locks, 1);
	// m->anchorage.locks++;
#ifdef ALASKA_CLASS_TRACKING
	alaska_classify_track(m->anchorage.object_class);
#endif
}

ALASKA_INLINE void anchorage_on_unlock(alaska_mapping_t *m) {
	atomic_dec(m->anchorage.locks, 1);
	// m->anchorage.locks--;
}

#define ALASKA_PERSONALITY_ON_LOCK(m) anchorage_on_lock(m)
#define ALASKA_PERSONALITY_ON_UNLOCK(m) anchorage_on_unlock(m)
