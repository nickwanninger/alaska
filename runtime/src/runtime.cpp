#include <alaska.h>
#include <alaska/translation_types.h>
#include <alaska/Arena.hpp>

#include <sys/mman.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

#define MAP_ENTRY_SIZE 16 // enforced by a static assert in translation_types.h
#define TWO_MEGABYTES 0x200000LU

#ifdef ALASKA_DEBUG
#define log(fmt, args...) fprintf(stderr, "[alaska] " fmt, ##args)
#else
#define log(...)
#endif

// The arena ID is the second from top byte in a handle.
#define HANDLE_MARKER (1LLU << 63)

// How many map cells are there in the map table?
size_t map_size = 0;
// The memory for the alaska translation map. This lives at 0x200000 and is grown in 2mb chunks
static alaska_map_entry_t *map = NULL;

static alaska_map_entry_t *next_handle = NULL;


extern "C" void *alaska_alloc(size_t sz) {
	uint64_t handle = HANDLE_MARKER | (((uint64_t)next_handle) << 32);

	memset(next_handle, 0, MAP_ENTRY_SIZE);
	// max size of 2^31 ish, so this storage is fine
	next_handle->size = sz;
	next_handle->ptr = malloc(sz);

	next_handle++;
	return (void*)handle;
}
extern "C" void alaska_free(void *ptr) {
	auto handle = (uint64_t)ptr;
	handle &= ~HANDLE_MARKER;
	auto ent = (alaska_map_entry_t*)(handle >> 32);
	free(ent->ptr);
	memset(next_handle, 0, MAP_ENTRY_SIZE);
}

////////////////////////////////////////////////////////////////////////////
extern "C" void *alaska_guarded_lock(void *vhandle) {
	auto handle = (uint64_t)vhandle;
	handle &= ~HANDLE_MARKER;
	auto ent = (alaska_map_entry_t*)(handle >> 32);
	return ent->ptr;
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
	// TODO: do this using hugetlbfs :)
	next_handle = map = (alaska_map_entry_t*)mmap((void*)TWO_MEGABYTES, TWO_MEGABYTES, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	map_size = TWO_MEGABYTES / MAP_ENTRY_SIZE;
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
