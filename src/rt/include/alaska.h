#pragma once

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif


extern void *alaska_alloc(size_t sz) __attribute__((alloc_size(1), malloc, nothrow));
extern void alaska_free(void *ptr);

extern void *alaska_pin(void *);
extern void alaska_unpin(void *);
extern void alaska_barrier(void);


#ifdef __cplusplus
}
#endif
