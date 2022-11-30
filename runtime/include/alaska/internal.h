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
} alaska_mapping_t;

extern alaska_mapping_t *alaska_map;
extern size_t alaska_map_size;

#define MAP_ENTRY_SIZE sizeof(alaska_mapping_t)  // enforced by a static assert in translation_types.h
#define MAP_GRANULARITY 0x200000LU               // minimum size of a "huge page" if we ever get around to that
#define HANDLE_MARKER (1LLU << 63)
#define GET_ENTRY(handle) ((alaska_mapping_t *)((((uint64_t)(handle)) & ~HANDLE_MARKER) >> 32))
#define GET_OFFSET(handle) ((off_t)(handle)&0xFFFFFFFF)

#define ENT_GET_CANONICAL(ent) (((off_t)(ent) - (off_t)(alaska_map)) / MAP_ENTRY_SIZE)
#define GET_CANONICAL(handle) ENT_GET_CANONICAL(GET_ENTRY(handle))

#ifdef __cplusplus
}
#endif
