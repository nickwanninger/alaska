#include <alaska.h>
#include <alaska/translation_types.h>
#include <alaska/Arena.hpp>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

// #include <jemalloc/jemalloc.h>
#define ALWAYS_INLINE inline __attribute__((always_inline))

#ifdef ALASKA_DEBUG
#define log(fmt, args...) fprintf(stderr, "[alaska] " fmt, ##args)
#else
#define log(...)
#endif

// The arena ID is the second from top byte in a handle.
#define HANDLE_MARKER (1LLU << 63)

static alaska::Arena *arenas[ALASKA_MAX_ARENAS];


void alaska_die(const char *msg) {
  fprintf(stderr, "alaska_die: %s\n", msg);
  abort();
}

extern "C" void *alaska_alloc(size_t sz) { return (void *)arenas[0]->allocate(sz); }
extern "C" void alaska_free(void *ptr) { return arenas[0]->free((alaska_handle_t)ptr); }

static uint32_t access_time = 0;

////////////////////////////////////////////////////////////////////////////
extern "C" void *alaska_guarded_lock(void *vhandle) {
  alaska_handle_t handle = (alaska_handle_t)vhandle;
  int bin = ALASKA_GET_BIN(handle);
  alaska_map_entry_t *entry;
  alaska_handle_t th = (handle << 9) >> (9 + 6);
  entry = (alaska_map_entry_t *)th;

	entry->locks++;
	entry->last_access = access_time++;
  off_t offset = (off_t)handle & (off_t)((2 << (bin + 5)) - 1);
  void *addr = (void *)((off_t)entry->ptr + offset);
  return addr;
}


extern "C" void alaska_guarded_unlock(void *ptr) {
}

extern "C" void *alaska_lock(void *ptr) {
  uint64_t h = (uint64_t)ptr;
  if ((h & HANDLE_MARKER) != 0) {
    return alaska_guarded_lock(ptr);
  }
  return ptr;
}

void alaska_unlock(void *ptr) {
  uint64_t h = (uint64_t)ptr;
  if ((h & HANDLE_MARKER) != 0) {
		alaska_guarded_unlock(ptr);
  }
}

void alaska_barrier(void) { log("--- barrier ---\n"); }



static void __attribute__((constructor)) alaska_init(void) {

	printf("0x%zx per 2mb block\n", (0x200000LU / sizeof(alaska_map_entry_t)) - 1);

  // Zero out the arenas array so invalid lookups don't cause UB
  for (int i = 0; i < ALASKA_MAX_ARENAS; i++) {
    arenas[i] = NULL;
  }

  // allocate the default arena
  arenas[0] = new alaska::Arena(0);
}


long alaska_tlb_hits = 0;
long alaska_tlb_misses = 0;

static void __attribute__((destructor)) alaska_deinit(void) {
}

extern "C" void *alaska_alloc_map_frame(int level, size_t entry_size, int size) {
  void *p = calloc(entry_size, size);
  return p;
}


void *operator new(size_t size) {
  void *ptr = malloc(size);
  return ptr;
}

void operator delete(void *ptr) { free(ptr); }
