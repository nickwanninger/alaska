#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>


#ifdef ALASKA_PERSONALITY_NONE
#include <alaska/personality/none.h>
#endif

#ifdef ALASKA_PERSONALITY_ANCHORAGE
#include <alaska/personality/anchorage.h>
#endif

#include <alaska/list_head.h>

// This is the interface for personalities. It may seem like a hack, but it
// helps implementation and performance (inline stuff in alaska_lock in lock.c)
#ifndef ALASKA_PERSONALITY_ON_LOCK
#define ALASKA_PERSONALITY_ON_LOCK(mapping)  // ... nothing ...
#endif

#ifndef ALASKA_PERSONALITY_ON_UNLOCK
#define ALASKA_PERSONALITY_ON_UNLOCK(mapping)  // ... nothing ...
#endif

// extra fields to place in the handle table
#ifndef ALASKA_PERSONALITY_FIELDS
#define ALASKA_PERSONALITY_FIELDS // ... nothing ...
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
    unsigned offset : 32;  // the offset into the handle
    unsigned handle : 31;  // the translation in the translation table
    unsigned flag : 1;     // the high bit indicates the `ptr` is a handle
  };
  void *ptr;
} handle_t;

typedef struct {
	// Cache line 1:
  void *ptr; // backing memory
  uint32_t locks; // how many users?
  // size: how big the backing memory is
  uint32_t size;

	// Cache line 2:
  ALASKA_PERSONALITY_FIELDS; // personality fields.
} alaska_mapping_t;

// src/lock.c
void *alaska_lock(void *restrict ptr);
void alaska_unlock(void *restrict ptr);


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

#define HANDLE_MARKER (1LLU << 63)
#define GET_ENTRY(handle) ((alaska_mapping_t *)((((uint64_t)(handle)) & ~HANDLE_MARKER) >> 32))
#define GET_OFFSET(handle) ((off_t)(handle)&0xFFFFFFFF)


// stolen from redis, it's just a nicer interface :)
#define atomic_inc(var, count) __atomic_add_fetch(&var, (count), __ATOMIC_RELAXED)
#define atomic_get_inc(var, oldvalue_var, count)                        \
  do {                                                                  \
    oldvalue_var = __atomic_fetch_add(&var, (count), __ATOMIC_RELAXED); \
  } while (0)
#define atomic_dec(var, count) __atomic_sub_fetch(&var, (count), __ATOMIC_RELAXED)
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
#endif
