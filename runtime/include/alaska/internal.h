#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "./config.h"

#ifdef ALASKA_SERVICE_NONE
#include <alaska/service/none.h>
#endif

#ifdef ALASKA_SERVICE_ANCHORAGE
#include <alaska/service/anchorage.h>
#endif

#ifdef ALASKA_SERVICE_NONE
#include <alaska/service/none.h>
#endif

#include <alaska/list_head.h>


// extra fields to place in the handle table
#ifndef ALASKA_SERVICE_FIELDS
#define ALASKA_SERVICE_FIELDS  // ... nothing ...
#endif


#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)
#define round_up(x, y) (((x) + (y)-1) & ~((y)-1))

#define ALASKA_EXPORT __attribute__((visibility("default")))

#define ALASKA_INLINE __attribute__((always_inline))




// This file contains structures that the the translation subsystem uses
#ifdef __cplusplus
extern "C" {
#endif


extern void alaska_dump_backtrace(void);

#ifdef ALASKA_SANITY_CHECK
#define ALASKA_SANITY(c, msg, ...)                                                              \
  do {                                                                                          \
    if (!(c)) { /* if the check is not true... */                                               \
      fprintf(stderr, "\x1b[31m-----------[ Alaska Sanity Check Failed ]-----------\x1b[0m\n"); \
      fprintf(stderr, "%s line %d\n", __FILE__, __LINE__);                                      \
      fprintf(stderr, "Check, `%s`, failed\n", #c, ##__VA_ARGS__);                              \
      fprintf(stderr, msg "\n", ##__VA_ARGS__);                                                 \
      alaska_dump_backtrace();                                                                  \
      fprintf(stderr, "\x1b[31mContinuing and hoping stuff doesn't break.\x1b[0m\n");           \
      fprintf(stderr, "\x1b[31m----------------------------------------------------\x1b[0m\n"); \
    }                                                                                           \
  } while (0);
#else
#define ALASKA_SANITY(c, msg, ...) /* do nothing if it's disabled */
#endif
// exit(EXIT_FAILURE);


// The definition of what a handle's bits mean when they are used like a pointer
typedef union {
  struct {
    unsigned long offset : ALASKA_OFFSET_BITS;             // the offset into the handle
    unsigned long handle : (64 - 1 - ALASKA_OFFSET_BITS);  // the translation in the translation table
    unsigned flag : 1;                                     // the high bit indicates the `ptr` is a handle
  };
  void *ptr;
} handle_t;

typedef struct {
  // Cache line 1:
  void *ptr;  // backing memory
  // size: how big the backing memory is
  uint32_t size;  // How big is the handle's memory?
#ifdef ALASKA_CLASS_TRACKING
  uint8_t object_class;
#endif
  // Cache line 2:
  ALASKA_SERVICE_FIELDS;  // personality fields.
} alaska_mapping_t;           // __attribute__((packed));

// src/lock.c
void *alaska_lock(void *ptr);
void alaska_unlock(void *ptr);

alaska_mapping_t *alaska_lookup(void *ptr);

// src/halloc.c
void alaska_halloc_init(void);
void alaska_halloc_deinit(void);

// src/table.c
void alaska_table_init(void);    // initialize the global table
void alaska_table_deinit(void);  // teardown the global table
unsigned alaska_table_get_canonical(alaska_mapping_t *);
alaska_mapping_t *alaska_table_from_canonical(unsigned canon);
// allocate a handle id
alaska_mapping_t *alaska_table_get(void);
// free a handle id
void alaska_table_put(alaska_mapping_t *ent);
alaska_mapping_t *alaska_table_begin(void);
alaska_mapping_t *alaska_table_end(void);


#ifdef ALASKA_CLASS_TRACKING
void alaska_classify_init(void);
void alaska_classify_deinit(void);
#endif


// Generate a hash that can be displayed
// extern char *alaska_randomart(const unsigned char *raw, size_t raw_len, size_t fldbase, const char *palette);

typedef uint32_t alaska_spinlock_t;
#define ALASKA_SPINLOCK_INIT 0
inline void alaska_spin_lock(volatile alaska_spinlock_t *lock) {
  while (__sync_lock_test_and_set(lock, 1)) {
    // spin away
  }
}
inline int alaska_spin_try_lock(volatile alaska_spinlock_t *lock) { return __sync_lock_test_and_set(lock, 1) ? -1 : 0; }
inline void alaska_spin_unlock(volatile alaska_spinlock_t *lock) { __sync_lock_release(lock); }

#define HANDLE_MARKER (1LLU << 63)
#define GET_ENTRY(handle) ((alaska_mapping_t *)((((uint64_t)(handle)) & ~HANDLE_MARKER) >> 32))
#define GET_OFFSET(handle) ((off_t)(handle)&0xFFFFFFFF)



struct alaska_lock_frame {
  struct alaska_lock_frame *prev;
  uint64_t count;
  void *locked[];
};
// In barrier.c
extern __thread struct alaska_lock_frame *alaska_lock_root_chain;

void alaska_barrier_add_thread(pthread_t *thread);
void alaska_barrier_remove_thread(pthread_t *thread);



// stolen from redis, it's just a nicer interface :)
#define atomic_inc(var, count) __atomic_add_fetch(&var, (count), __ATOMIC_SEQ_CST)
#define atomic_get_inc(var, oldvalue_var, count)                        \
  do {                                                                  \
    oldvalue_var = __atomic_fetch_add(&var, (count), __ATOMIC_RELAXED); \
  } while (0)
#define atomic_dec(var, count) __atomic_sub_fetch(&var, (count), __ATOMIC_SEQ_CST)
#define atomic_get(var, dstvar)                       \
  do {                                                \
    dstvar = __atomic_load_n(&var, __ATOMIC_RELAXED); \
  } while (0)
#define atomic_set(var, value) __atomic_store_n(&var, value, __ATOMIC_RELAXED)
#define atomic_get_sync(var, dstvar)                  \
  do {                                                \
    dstvar = __atomic_load_n(&var, __ATOMIC_SEQ_CST); \
  } while (0)
#define atomic_set_sync(var, value) __atomic_store_n(&var, value, __ATOMIC_SEQ_CST)

#ifdef __cplusplus
}

namespace alaska {
	using Mapping = alaska_mapping_t;
};


#endif
