#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct anchorage_metadata {
#ifdef ALASKA_CLASS_TRACKING
	uint8_t object_class;
#endif
};

#define ALASKA_PERSONALITY_FIELDS struct anchorage_metadata anchorage;

#ifdef __cplusplus
}
#endif
