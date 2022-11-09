#include <alaska.h>
#include <alaska/translation_types.h>
#include <alaska/rbtree.h>
#include <alaska/set.h>
#include <alaska/Arena.hpp>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

// #include <jemalloc/jemalloc.h>
#define ALWAYS_INLINE inline __attribute__((always_inline))
#define GRN "\e[0;32m"
#define GRY "\e[0;90m"
#define RESET "\e[0m"
#define PFX "alaska: "

#ifdef ALASKA_DEBUG
#define log(fmt, args...) fprintf(stderr, PFX fmt, ##args)
#else
#define log(...)
#endif

#define round_up(x, y) (((x) + (y)-1) & ~((y)-1))


// The arena ID is the second from top byte in a handle.
#define HANDLE_MASK (0xFF00LU << 48)
#define ARENA_MASK (0x00FFLU << 48)
// Return the arena id for a handle
#define HANDLE_ARENA(handle) (((handle) >> 48) & 0xFF)
// Given an arena id, return the handle w/ the bits set
#define IN_ARENA(handle, arena) (((handle) | ARENA_MASK) >> 48)
#define WITH_ARENA(aid, h) ((h) | HANDLE_MASK | (((uint64_t)(aid)) << 48))


// The top bit of an address indicates that it is in fact a handle
#define HANDLE_MARKER (1LLU << 64)

static alaska::Arena *arenas[ALASKA_MAX_ARENAS];

// static uint64_t now_ns() {
//   struct timespec spec;
//   if (clock_gettime(1, &spec) == -1) { /* 1 is CLOCK_MONOTONIC */
//     abort();
//   }
//
//   return spec.tv_sec * (1000 * 1000 * 1000) + spec.tv_nsec;
// }


void alaska_die(const char *msg) {
  fprintf(stderr, "alaska_die: %s\n", msg);
  abort();
}

static uint64_t pin_count = 0;
static uint64_t unpin_count = 0;
static uint64_t dyncall_count = 0;



extern "C" void *alaska_alloc(size_t sz) { return (void *)arenas[0]->allocate(sz); }

extern "C" void alaska_free(void *ptr) { return arenas[0]->free((alaska_handle_t)ptr); }


////////////////////////////////////////////////////////////////////////////
extern "C" void *alaska_guarded_pin(void *handle) {
  // This function *requires* that the input is a handle. Otherwise the program will crash
  auto ptr = arenas[0]->pin((alaska_handle_t)handle);
  // log("  pin %p\n", handle);
  return ptr;
}


extern "C" void alaska_guarded_unpin(void *ptr) {
  // This function *requires* that the input is a handle. Otherwise the program will crash
  // log("unpin %p\n", ptr);
  // alaska_die("Unimplemented function");
  arenas[0]->unpin((alaska_handle_t)ptr);
}

extern "C" void *alaska_pin(void *ptr) {
  uint64_t h = (uint64_t)ptr;
  if ((h & HANDLE_MASK) != 0) {
    return alaska_guarded_pin(ptr);
  } else {
    dyncall_count++;
  }
  return ptr;
}

void alaska_unpin(void *ptr) {
  uint64_t h = (uint64_t)ptr;
  if ((h & HANDLE_MASK) != 0) {
    alaska_guarded_unpin(ptr);
  } else {
    dyncall_count++;
  }
}


////////////////////////////////////////////////////////////////////////////
void alaska_barrier(void) { log("--- barrier ---\n"); }



static void __attribute__((constructor)) alaska_init(void) {
  // Zero out the arenas array so invalid lookups don't cause UB
  for (int i = 0; i < ALASKA_MAX_ARENAS; i++) {
    arenas[i] = NULL;
  }

  // allocate the default arena
  arenas[0] = new alaska::Arena(0);
}

static void __attribute__((destructor)) alaska_deinit(void) {
  log("=================================\n");
  log("ALASKA_PINS:          %llu\n", pin_count);
  log("ALASKA_UNPINS:        %llu\n", unpin_count);
  log("ALASKA_CALLS:         %llu\n", dyncall_count);
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