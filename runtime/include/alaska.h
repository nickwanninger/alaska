#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <alaska/config.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef uint64_t alaska_handle_t;


extern void *halloc(size_t sz) __attribute__((alloc_size(1), malloc, nothrow));
extern void *hcalloc(size_t nmemb, size_t size);
extern void *hrealloc(void *handle, size_t sz);
extern void hfree(void *ptr);


// "Go do something in the runtime", whatever that means in the active service
extern void alaska_barrier(void);


// Helper functions:
// get the current rss in kb
extern long alaska_translate_rss_kb(void);
// nanoseconds
extern unsigned long alaska_timestamp(void);

#ifdef ALASKA_SERVICE_ANCHORAGE
// Manufacture locality using `entrypoint` as a root in a conservative reachability
extern void anchorage_manufacture_locality(void *entrypoint);
#endif



#ifdef __cplusplus
}
#endif

// allow functions in translated programs to be skipped
#define alaska_rt __attribute__((section("$__ALASKA__$"), noinline))