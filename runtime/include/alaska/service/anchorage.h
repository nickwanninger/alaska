#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct anchorage_metadata {
	uint32_t locks;
};

#define ALASKA_SERVICE_FIELDS struct anchorage_metadata anchorage;

#ifdef __cplusplus
}
#endif
