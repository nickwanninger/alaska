#pragma once

#include <stdlib.h>
#include <stdint.h>

// This file contains structures that the the translation subsystem uses
#ifdef __cplusplus
extern "C" {
#endif


#define ALASKA_INDICATOR (1LU << 63)
#define ALASKA_AID(aid) (((unsigned long)(aid)) << 60)  // 3 bits
#define ALASKA_BIN(bin) (((unsigned long)(bin)) << 55)  // 5 bits


#define ALASKA_GET_AID(h) (((h) >> 60) & 0b111)
#define ALASKA_GET_BIN(h) (((h) >> 55) & 0b11111)

typedef struct {
  void *ptr;
  uint32_t locks;
  uint8_t arena;
  uint16_t size;
  // other stuff...
} alaska_map_entry_t;

typedef alaska_map_entry_t *(*alaska_map_driller_t)(uint64_t, uint64_t *, int);

extern void *alaska_alloc_map_frame(int level, size_t entry_size, int size);

#ifdef __cplusplus
}
#endif
