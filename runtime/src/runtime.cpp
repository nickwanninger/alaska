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

#define likely(x) __builtin_expect((x), 1)

#define MAP_ENTRY_SIZE 16 // enforced by a static assert in translation_types.h
#define MAP_GRANULARITY 0x100000LU

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
// where to start looking to allocate a new handle
static alaska_map_entry_t *next_handle = NULL;


#define GET_ENTRY(handle) ((alaska_map_entry_t*)((((uint64_t)(handle)) & ~HANDLE_MARKER) >> 32))

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
	auto ent = GET_ENTRY(ptr);
	free(ent->ptr);
	memset(ent, 0, MAP_ENTRY_SIZE);
}

////////////////////////////////////////////////////////////////////////////
extern "C" void *alaska_guarded_lock(void *ptr) {
	auto ent = GET_ENTRY(ptr);
	ent->locks++;
	return ent->ptr;
}


extern "C" void alaska_guarded_unlock(void *ptr) {
	auto ent = GET_ENTRY(ptr);
	ent->locks--;
}

extern "C" void *alaska_lock(void *ptr) {
  uint64_t h = (uint64_t)ptr;
  if (likely((h & HANDLE_MARKER) != 0)) {
    return alaska_guarded_lock(ptr);
  }
  return ptr;
}

void alaska_unlock(void *ptr) {
  uint64_t h = (uint64_t)ptr;
  if (likely((h & HANDLE_MARKER) != 0)) {
		alaska_guarded_unlock(ptr);
  }
}

void alaska_barrier(void) { log("--- barrier ---\n"); }





static void __attribute__((constructor)) alaska_init(void) {
	size_t sz = MAP_GRANULARITY * 8;
	// TODO: do this using hugetlbfs :)
	next_handle = map = (alaska_map_entry_t*)mmap((void*)MAP_GRANULARITY, sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	map_size = sz / MAP_ENTRY_SIZE;
	printf("Initialize %zu map cells at %p\n", map_size, map);
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
