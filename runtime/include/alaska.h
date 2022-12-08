#pragma once

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef uint64_t alaska_handle_t;

#define ALASKA_TLB_SIZE


extern void *halloc(size_t sz) __attribute__((alloc_size(1), malloc, nothrow));
extern void *hcalloc(size_t nmemb, size_t size);
extern void *hrealloc(void *handle, size_t sz);
extern void hfree(void *ptr);
// "Go do something in the runtime", whatever that means right now
extern void alaska_barrier(void);

#ifdef __cplusplus
}
#endif

// allow functions in translated programs to be skipped
#define alaska_rt __attribute__((section("$__ALASKA__$"), noinline))
