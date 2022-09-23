#include <alaska.h>
#include <alaska/rbtree.h>
#include <alaska/set.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

// #include <jemalloc/jemalloc.h>

#define GRN "\e[0;32m"
#define GRY "\e[0;90m"
#define RESET "\e[0m"
#define PFX GRN "alaska: " RESET

#ifdef ALASKA_DEBUG
#define log(fmt, args...) printf(PFX fmt RESET, ##args)
#else
#define log(...)
#endif

#define round_up(x, y) (((x) + (y)-1) & ~((y)-1))

struct alaska_handle_s {
  struct rb_node node;  // the link to the rbtree
  uint64_t handle;      // the handle of this allocation
  uint64_t size;        // the size of this allocation
  uint8_t data;
};

// The arena ID is the second from top byte in a handle.
#define MAX_ARENAS 255
#define HANDLE_MASK (0xFF00LU << 48)
#define ARENA_MASK (0x00FFLU << 48)
// Return the arena id for a handle
#define HANDLE_ARENA(handle) ((handle) >> 48)
// Given an arena id, return the handle w/ the bits set
#define IN_ARENA(handle, arena) (((handle) | ARENA_MASK) >> 48)

static alaska_arena_t *arenas[MAX_ARENAS];

uint64_t now_ns() {
  struct timespec spec;
  if (clock_gettime(1, &spec) == -1) { /* 1 is CLOCK_MONOTONIC */
    abort();
  }

  return spec.tv_sec * (1000 * 1000 * 1000) + spec.tv_nsec;
}

void alaska_die(const char *msg) {
  fprintf(stderr, "alaska_die: %s\n", msg);
  abort();
}

static uint64_t pin_count = 0;
static uint64_t unpin_count = 0;
static struct rb_root handle_table;
static uint64_t next_handle = 0x1000;

static alaska_handle_t *alaska_find(uint64_t va) {
  // walk...
  struct rb_node **n = &(handle_table.rb_node);
  struct rb_node *parent = NULL;

  int steps = 0;

  /* Figure out where to put new node */
  while (*n != NULL) {
    alaska_handle_t *r = rb_entry(*n, alaska_handle_t, node);

    off_t start = (off_t)r->handle;
    off_t end = start + r->size;
    parent = *n;

    steps++;

    if (va < start) {
      n = &((*n)->rb_left);
    } else if (va >= end) {
      n = &((*n)->rb_right);
    } else {
      return r;
    }
  }

  return NULL;
}

static inline int __insert_callback(struct rb_node *n, void *arg) {
  alaska_handle_t *allocation = arg;
  alaska_handle_t *other = rb_entry(n, alaska_handle_t, node);
  // do we go left right or here?
  long result = (long)allocation->handle - (long)other->handle;

  if (result < 0) {
    return RB_INSERT_GO_LEFT;
  }
  if (result > 0) {
    return RB_INSERT_GO_RIGHT;
  }
  return RB_INSERT_GO_HERE;
}

void *alaska_alloc(size_t sz) {
  alaska_handle_t *handle = malloc(sizeof(alaska_handle_t) + sz);
  handle->size = sz;
  handle->handle = (uint64_t)next_handle | HANDLE_MASK;
  next_handle += round_up(16, sz);

  rb_insert(&handle_table, &handle->node, __insert_callback, (void *)handle);
  log("alloc %p\n", (void *)handle->handle);
  return (void *)handle->handle;
}

void alaska_free(void *ptr) {
  uint64_t handle = (uint64_t)ptr;

  if ((handle & HANDLE_MASK) != 0) {
    // uint64_t start = now_ns();
    alaska_handle_t *h = alaska_find(handle);
    if (h == NULL) {
      alaska_die("Attempt to free nonexistent handle");
      return;
    }
    rb_erase(&h->node, &handle_table);
    log("free  %p -> %p\n", ptr, &h->data);
    free(h);
  }
}

// Given a handle, fill the out variables w/ the relevant
// structures and return 0 on success. Returns -errno otherwise
static int find_arena_and_handle(uint64_t h, alaska_handle_t **o_handle, alaska_arena_t **o_arena) {
  if (h & ARENA_MASK != ARENA_MASK) return -EINVAL;
  uint8_t aid = HANDLE_ARENA(h);
  log("find(0x%llx): aid=%d\n", h, aid);
  *o_arena = arenas[aid];

  if (arenas[aid] == NULL) return -ENOENT;

  return -ENOENT;
}




////////////////////////////////////////////////////////////////////////////
void *alaska_pin(void *ptr) {
  uint64_t h = (uint64_t)ptr;
  alaska_handle_t *handle;
  alaska_arena_t *arena;
  if ((h & HANDLE_MASK) != 0) {
    if (find_arena_and_handle((uint64_t)ptr, &handle, &arena) != 0) {
      alaska_die("Failed to pin!");
      return NULL;
    }

    arena->pinned(arena, handle);
    return handle->data;
  }
  return ptr;
}


////////////////////////////////////////////////////////////////////////////
void alaska_unpin(void *ptr) {
  log("unpin %p\n", ptr);

  uint64_t handle = (uint64_t)ptr;
  if ((handle & HANDLE_MASK) != 0) {
    alaska_handle_t *h = alaska_find(handle);
    if (h == NULL) alaska_die("unpin non handle");
    unpin_count += 1;
  }
}


////////////////////////////////////////////////////////////////////////////
void alaska_barrier(void) {
  log("--- barrier ---\n");
}



////////////////////////////////////////////////////////////////////////////
void alaska_arena_init(alaska_arena_t *arena) {
  int id = -1;
  for (id = 0; id < MAX_ARENAS; id++) {
    if (arenas[id] == NULL) break;
  }
  // Easy registration
  arenas[id] = arena;
  arena->id = id;
  arena->alloc = alaska_default_alloc;
  arena->free = alaska_default_free;
  arena->pinned = alaska_default_pinned;
  arena->unpinned = alaska_default_unpinned;
  return;
}

void *alaska_default_alloc(alaska_arena_t *arena, size_t sz) {
  return malloc(sz);
}
void alaska_default_free(alaska_arena_t *arena, void *ptr) {
  return free(ptr);
}
int alaska_default_pinned(alaska_arena_t *arena, alaska_handle_t *handle) {
  return 0;
}

int alaska_default_unpinned(alaska_arena_t *arena, alaska_handle_t *handle) {
  return 0;
}

static void __attribute__((constructor)) alaska_init(void) {
  // Zero out the arenas array so invalid lookups don't cause UB
  for (int i = 0; i < MAX_ARENAS; i++) {
    arenas[i] = NULL;
  }

  alaska_arena_t *base_arena = calloc(1, sizeof(*base_arena));
  alaska_arena_init(base_arena);

  handle_table = RB_ROOT;
}

static void __attribute__((destructor)) alaska_deinit(void) {
  log("=================================\n");
  log("ALASKA_PINS:          %llu\n", pin_count);
  log("ALASKA_UNPINS:        %llu\n", unpin_count);
  // printf("ALASKA_US_PINNING: %llu\n", ns_pinning / 1000);
  // printf("ALASKA_MS_PINNING: %llu\n", ns_pinning / 1000 / 1000);
}
