#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>


// This file contains structures that the the translation subsystem uses
#ifdef __cplusplus
extern "C" {
#endif

#ifdef ALASKA_SANITY_CHECK
#define ALASKA_SANITY(c, msg) 
#else
#define ALASKA_SANITY(c, msg) // do nothing if it's disabled
#endif


typedef struct {
  void *ptr;  // The raw backing memory of the handle
  uint32_t usage_timestamp;
  uint32_t size;
  uint16_t locks;  // How many users does this handle have?
  uint16_t flags;  // additional flags (idk, tbd)
} alaska_map_entry_t;


#ifdef __cplusplus
}
#endif
