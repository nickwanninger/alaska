#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct anchorage_metadata {
  int locks;
  uint8_t flags;
#define ANCHORAGE_FLAG_LAZY_FREE (1 << 0)
  uint64_t last_access_time;
};

#define ALASKA_SERVICE_FIELDS struct anchorage_metadata anchorage;

#ifdef __cplusplus
}
#endif
