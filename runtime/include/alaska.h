#pragma once

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef uint64_t alaska_handle_t;

#define ALASKA_TLB_SIZE

// =============================================

extern void *alaska_alloc(size_t sz) __attribute__((alloc_size(1), malloc, nothrow));
extern void alaska_free(void *ptr);

// These functions are inserted by the compiler pass. It is not
// recommended to use them directly
extern void *alaska_lock(void *handle);
extern void alaska_unlock(void *handle);
extern void alaska_barrier(void);

#ifdef __cplusplus
}
#endif

#ifdef __ACC__
// allow functions in translated programs to be skipped
#define alaska_rt __attribute__((section("$__ALASKA__$"), noinline))
#else
#define alaska_rt
#endif
