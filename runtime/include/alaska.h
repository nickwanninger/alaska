#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <alaska/config.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  // Track the depth of escape of a given thread. If this number is zero,
  // the thread is in 'managed' code and will eventually poll the barrier
  uint64_t escaped;
  // Why did this thread join? Is it joined?
  int join_status;
#define ALASKA_JOIN_REASON_NOT_JOINED -1        // This thread has not joined the barrier.
#define ALASKA_JOIN_REASON_SIGNAL 0        // This thread was signalled.
#define ALASKA_JOIN_REASON_SAFEPOINT 1     // This thread was at a safepoint
#define ALASKA_JOIN_REASON_ORCHESTRATOR 2  // THis thread was the orchestrator

  // ...
} alaska_thread_state_t;

extern __thread alaska_thread_state_t alaska_thread_state;


// Allocate a handle as well as it's backing memory if the runtime decides to do so.
// This is the main interface to alaska's handle system, as actually using handles is
// entirely transparent.
extern void *halloc(size_t sz) __attribute__((alloc_size(1), malloc, nothrow));

// calloc a handle, returning a zeroed array of nmemb elements, each of size `size`
extern void *hcalloc(size_t nmemb, size_t size);

// Realloc a handle. This function will always return the original handle, as it just needs to
// reallocate the backing memory. Thus, if your application relies on realloc returning something
// different, you should be careful!
extern void *hrealloc(void *handle, size_t sz);

// Free a given handle. Is a no-op if ptr=null
extern void hfree(void *ptr);

// "Go do something in the runtime", whatever that means in the active service. Most of the time,
// this will lead to a "slow" stop-the-world event, as the runtime must know all active/locked
// handles in the application as to avoid corrupting program state.
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


// #define TABLE_START 0x80000000LU
//
#if ALASKA_SIZE_BITS >= 17
#define TABLE_START (0x8000000000000000LLU >> (ALASKA_SIZE_BITS - ALASKA_SQUEEZE_BITS))
#else
#error "Cannot handle size bits less than 17"
#endif

// The alaska safepoint page lives immediately before the table start
// #define ALASKA_SAFEPOINT_PAGE ((void*)(TABLE_START - 0x1000))
#define ALASKA_SAFEPOINT_PAGE (void *)(0x7ffff000UL)
// The implementation of the safepoint poll function
extern void alaska_safepoint(void);


// Not a good function to call. This is always an external function in the compiler's eyes
extern void *__alaska_leak(void *);

#ifdef __cplusplus
}
#endif
