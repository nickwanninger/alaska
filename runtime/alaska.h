#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {

#define NOEXCEPT noexcept
#else
#define NOEXCEPT
#endif

// Allocate a handle as well as its backing memory if the runtime decides to do
// so. This is the main interface to alaska's handle system, as actually using
// handles is entirely transparent.
extern void *alaska_halloc(size_t sz) NOEXCEPT;  // __attribute__((alloc_size(1), malloc, nothrow));

// calloc a handle, returning a zeroed array of nmemb elements, each of size
// `size`
extern void *alaska_hcalloc(size_t nmemb, size_t size);

// Realloc a handle. This function will always return the original handle, as it
// just needs to reallocate the backing memory. Thus, if your application relies
// on realloc returning something different, you should be careful!
extern void *alaska_hrealloc(void *handle, size_t sz);

// Free a given handle. Is a no-op if ptr=null
extern void alaska_hfree(void *ptr);

extern size_t alaska_usable_size(void *ptr);

// "Go do something in the runtime", whatever that means in the active service.
// Most of the time, this will lead to a "slow" stop-the-world event, as the
// runtime must know all active/locked handles in the application as to avoid
// corrupting program state.
extern void alaska_barrier(void);

// Grab the current resident set size in kilobytes from the kernel
extern long alaska_translate_rss_kb(void);

// Get the current timestamp in nanoseconds. Mainly to be used
// for (end - start) time keeping and benchmarking
extern unsigned long alaska_timestamp(void);

struct alaska_blob_config {
  uintptr_t code_start, code_end;
  void *stackmap;
};
// In barrier.cpp
void alaska_blob_init(struct alaska_blob_config *cfg);

// Not a good function to call. This is always an external function in the
// compiler's eyes
extern void *__alaska_leak(void *);


void alaska_mark_for_fault_TEST(void *ptr);




enum AlaskaCtlOperation {
  ALASKA_CTL_MARK_FOR_FAULT,
  ALASKA_CTL_RUN_BARRIER,    // arg = struct alaska_barrier_config*
  ALASKA_CTL_COMPRESS_TEST,  // arg = handle
};

typedef enum {
  ALASKA_NOT_LINKED,  // The runtime is not linked in
  ALASKA_SUCCESS,
  ALASKA_INVALID,  // Invalid handle, invalid op, etc.
} AlaskaCtlResult;


struct alaska_barrier_config {
  void (*callback)(void *user);
  void *user;
};


__attribute__((noinline)) static inline AlaskaCtlResult alaska_ctl(enum AlaskaCtlOperation op,
                                                                   uint64_t arg) {
  // Internal call to the runtime. This must be marked as weak, as the application must be able to
  // compile without the runtime linked in.
  extern AlaskaCtlResult __alaska_ctl(enum AlaskaCtlOperation op, uint64_t arg)
      __attribute__((weak));
  if (__alaska_ctl) {
    return __alaska_ctl(op, arg);
  }
  return ALASKA_NOT_LINKED;
}


#ifdef __cplusplus
}
#endif
