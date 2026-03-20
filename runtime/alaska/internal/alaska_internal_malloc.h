#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *alaska_internal_malloc(size_t size);
void alaska_internal_free(void *ptr);
void *alaska_internal_calloc(size_t num, size_t size);

#ifdef __cplusplus
}
#endif
