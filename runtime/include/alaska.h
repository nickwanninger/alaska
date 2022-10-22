#pragma once

#include <alaska/rbtree.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
  void *backing_memory; // where the data sits while pinned
  struct rb_node node;  // the link to the rbtree
  uint64_t handle;      // the handle of this allocation
  uint64_t size;        // the size of this allocation
	long pin_depth;       // ++ on pin, -- on unpin. 0 means no users.
} alaska_handle_t;


struct alaska_arena;



#define ALASKA_TLB_SIZE

typedef struct alaska_arena_s {
  // the value placed in the 0xFFnn part of the address.
  uint16_t id;

  // Allocation of handles is a bump allocator for now. We may find a better
  // way later on based on use cases. "64 bits should be enough for everyone."
  long next_handle;

  // The red black tree which holds all the mappings.
  // Eventually a smarter structure would be better :)
  struct rb_root table;

  // The following functions are considered "hooks". Their main purpose
  // is to define the "personality" of a particular arena in the alaska
  // runtime. A personality mainly defines the resource management and
  // operation behind pinning and unpinning some class of handles.
  //
  // One example would be a "compression" personality, which upon
  // being unpinned, will compress the region of memory to save space.
  // When pinned is called, the personality would uncompress the memory
  // to allow the application to access it as if it were never compressed.
  //
  // Invoked to allocate the backing memory of a handle. This may be called
  // before `pinned` so it could operate as a no-op where the pin hook lazily
  // allocates the backing memory instead.
  void *(*alloc)(struct alaska_arena_s *, size_t);

  // Invoked when a handle is freed, to deallocate the backing data.
  // This is called after a call to unpinned. (ex: If `unpinned` releases
  // memory, this function could be a no-op)
  void (*free)(struct alaska_arena_s *, void *);

  // Invoked when a handle was not pinned, and transitioned into pinned.
  // If this function returns 0, it is required that the handle has backing
  // memory that is transparently accessible to the application.
  int (*pinned)(struct alaska_arena_s *, alaska_handle_t *);

  // Invoked when a handle was pinned, and has been unpinned
  // by all parties. It is perfectly fine to invalidate the
  // backing memory of the handle in this function.
  int (*unpinned)(struct alaska_arena_s *, alaska_handle_t *);

} alaska_arena_t;


// Initialize and register an arena. Call this before setting any hooks up.
extern void alaska_arena_init(alaska_arena_t *arena);

// wrappers around libc's malloc/free
extern void *alaska_default_alloc(alaska_arena_t *arena, size_t sz);
extern void alaska_default_free(alaska_arena_t *arena, void *ptr);
extern int alaska_default_pinned(alaska_arena_t *arena, alaska_handle_t *);
extern int alaska_default_unpinned(alaska_arena_t *arena, alaska_handle_t *);

// =============================================

extern void *alaska_alloc(size_t sz) __attribute__((alloc_size(1), malloc, nothrow));
extern void *alaska_arena_alloc(size_t sz, alaska_arena_t *arena) __attribute__((alloc_size(1), malloc, nothrow));
extern void alaska_free(void *ptr);

// These functions are inserted by the compiler pass. It is not
// recommended to use them directly
extern void *alaska_pin(void *handle);
extern void alaska_unpin(void *handle);
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
