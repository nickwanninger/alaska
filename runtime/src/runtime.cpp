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
#include <assert.h>

#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)

#define MAP_ENTRY_SIZE 16 // enforced by a static assert in translation_types.h
#define MAP_GRANULARITY 0x100000LU // minimum size of a "huge page" if we ever get around to that
#define HANDLE_MARKER (1LLU << 63)
#define GET_ENTRY(handle) ((alaska_map_entry_t*)((((uint64_t)(handle)) & ~HANDLE_MARKER) >> 32))

// How many map cells are there in the map table?
size_t map_size = 0;
// The memory for the alaska translation map. This lives at 0x200000 and is grown in 2mb chunks
static alaska_map_entry_t *map = NULL;
// where to start looking to allocate a new handle
static alaska_map_entry_t *next_handle = NULL;



extern "C" void *alaska_alloc(size_t sz) {
	assert(sz < (1LLU << 32));
	uint64_t handle = HANDLE_MARKER | (((uint64_t)next_handle) << 32);

	memset(next_handle, 0, MAP_ENTRY_SIZE);

	// max size of 2^31 ish, so storing this in an int is save.
	next_handle->size = sz;
	next_handle->ptr = NULL; // don't allocate now. Do it later.

	next_handle++;
	return (void*)handle;
}

// Reallocate a handle
extern "C" void *alaska_realloc(void *handle, size_t sz) {
	auto ent = GET_ENTRY(handle);
	ent->ptr = realloc(ent->ptr, sz);
	ent->size = sz;
	// realloc in alaska will always return the same pointer...
	// Ahh the benefits of using handles!
	//
	// hopefully, nobody is relying on the output of realloc changing :I
	return handle;
}

extern "C" void alaska_free(void *ptr) {
	auto ent = GET_ENTRY(ptr);
	assert(ent->locks == 0);
	free(ent->ptr);
	memset(ent, 0, MAP_ENTRY_SIZE);
}


// Alaska's version of a soft page fault. Note the "noinline". This is because this
// is a slow path, and we don't really care about performance, as this function
// may go out to disk, decompress, decrypt, or something else more interesting :)
//
// This, and alaska_barrier are the main locations where the runtime can be customized
extern "C" __declspec(noinline) void alaska_fault(alaska_map_entry_t *ent) {
	// lazy allocate for now.
	ent->ptr = malloc(ent->size);
}

// This just means that the application has decided it is willing to let alaska
// potentially stop the world and do what it needs to do. This function doesn't
// do anything yet, but it could :)
extern "C" __declspec(noinline) void alaska_barrier(void) {
	// Work through `map` and do something fun
}

extern "C" void *alaska_guarded_lock(void *ptr) {
	auto ent = GET_ENTRY(ptr);
	// if the pointer is NULL, we need to perform a "handle fault"
	if (unlikely(ent->ptr == NULL)) {
		alaska_fault(ent);
	}

	ent->locks++;
	return ent->ptr;
}


extern "C" void alaska_guarded_unlock(void *ptr) {
	auto ent = GET_ENTRY(ptr);
	ent->locks--;
	if (ent->locks == 0) {
		// Maybe do something fun?
	}
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




static void __attribute__((constructor)) alaska_init(void) {
	size_t sz = MAP_GRANULARITY * 8;
	// TODO: do this using hugetlbfs :)
	next_handle = map = (alaska_map_entry_t*)mmap((void*)MAP_GRANULARITY, sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	map_size = sz / MAP_ENTRY_SIZE;
}


long alaska_tlb_hits = 0;
long alaska_tlb_misses = 0;

static void __attribute__((destructor)) alaska_deinit(void) {
	munmap(map, map_size * MAP_ENTRY_SIZE);
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
