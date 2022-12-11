#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <alaska/list_head.h>


// This file contains structures that the the translation subsystem uses
#ifdef __cplusplus
extern "C" {
#endif

#ifdef ALASKA_SANITY_CHECK
#define ALASKA_SANITY(c, msg)                                                                   \
  do {                                                                                          \
    if (!(c)) { /* if the check is not true... */                                               \
      fprintf(stderr, "\x1b[31m-----------[ Alaska Sanity Check Failed ]-----------\x1b[0m\n"); \
      fprintf(stderr, "%s line %d\n", __FILE__, __LINE__);                                      \
      fprintf(stderr, "%s (`%s` is false)\n", msg, #c);                                         \
      fprintf(stderr, "\x1b[31m----------------------------------------------------\x1b[0m\n"); \
      exit(EXIT_FAILURE);                                                                       \
    }                                                                                           \
  } while (0);
#else
#define ALASKA_SANITY(c, msg) /* do nothing if it's disabled */
#endif


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



// src/table.c
void alaska_table_init(void); // initialize the global table
void alaska_table_deinit(void); // teardown the global table

unsigned alaska_table_get_canonical(alaska_mapping_t *);
alaska_mapping_t *alaska_table_from_canonical(unsigned canon);
// allocate a handle id
alaska_mapping_t *alaska_table_get(void);
// free a handle id
void alaska_table_put(alaska_mapping_t *ent);


#define HANDLE_MARKER (1LLU << 63)
#define GET_ENTRY(handle) ((alaska_mapping_t *)((((uint64_t)(handle)) & ~HANDLE_MARKER) >> 32))
#define GET_OFFSET(handle) ((off_t)(handle)&0xFFFFFFFF)


// stolen from redis, it's just a nicer interface :)
#define atomic_inc(var,count) __atomic_add_fetch(&var,(count),__ATOMIC_RELAXED)
#define atomic_get_inc(var,oldvalue_var,count) do { \
    oldvalue_var = __atomic_fetch_add(&var,(count),__ATOMIC_RELAXED); \
} while(0)
#define atomic_dec(var,count) __atomic_sub_fetch(&var,(count),__ATOMIC_RELAXED)
#define atomic_get(var,dstvar) do { \
    dstvar = __atomic_load_n(&var,__ATOMIC_RELAXED); \
} while(0)
#define atomic_set(var,value) __atomic_store_n(&var,value,__ATOMIC_RELAXED)
#define atomic_get_sync(var,dstvar) do { \
    dstvar = __atomic_load_n(&var,__ATOMIC_SEQ_CST); \
} while(0)
#define atomic_set_sync(var,value) \
    __atomic_store_n(&var,value,__ATOMIC_SEQ_CST)

#ifdef __cplusplus
}
#endif
