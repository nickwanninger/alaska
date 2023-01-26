#pragma once


#include <alaska/internal.h>

#ifdef __cplusplus
extern "C" {
#endif

// Anchorage is a custom allocator for use in Alaska that is
// enabled by Handles. The main functionality it implements
// is tracking and defragmentation

// void *anchorage_malloc(size_t sz);
// extern void anchorage_on_lock(alaska_mapping_t *m);
// extern void anchorage_on_unlock(alaska_mapping_t *m);

// #define ALASKA_PERSONALITY_ON_LOCK(m) anchorage_on_lock(m)
// #define ALASKA_PERSONALITY_ON_LOCK(m) anchorage_on_lock(m)

// If this macro is defined, this header is being included in
// lock.c, and any code that should be inlined ought to be
// declared as static inline
#ifdef __ALASKA_LOCK_INLINE


#endif

#ifdef __cplusplus
}
#endif
