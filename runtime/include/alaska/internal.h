#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <alaska/list_head.h>

#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)
#define round_up(x, y) (((x) + (y)-1) & ~((y)-1))

#define ALASKA_EXPORT __attribute__((visibility("default")))

typedef union {
  struct {
    unsigned offset : 32;  // the offset into the handle
    unsigned handle : 31;  // the translation in the translation table
    unsigned flag : 1;     // the high bit indicates the `ptr` is a handle
  };
  void* ptr;
} handle_t;


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


typedef struct {
  void *ptr;  // The raw backing memory of the handle
  uint32_t usage_timestamp;
  // struct list_head lru_ent;
  uint32_t size;
  uint64_t locks;  // How many users does this handle have?
  // uint16_t flags;  // additional flags (idk, tbd)
#ifdef ALASKA_CLASS_TRACKING
  uint8_t object_class;
#endif
} alaska_mapping_t;

// src/lock.c
void *alaska_lock(void *restrict ptr);
void alaska_unlock(void *restrict ptr);

alaska_mapping_t *alaska_get_mapping(void *restrict ptr);
void *alaska_translate(void *restrict ptr, alaska_mapping_t *m);
// bookkeeping functions inserted by the compiler
void alaska_track_access(alaska_mapping_t *m);
void alaska_track_lock(alaska_mapping_t *m);
void alaska_track_unlock(alaska_mapping_t *m);


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
