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
#define MAX_ARENAS 255
#define HANDLE_MASK (0xFF00LU << 48)
#define ARENA_MASK (0x00FFLU << 48)
// Return the arena id for a handle
#define HANDLE_ARENA(handle) (((handle) >> 48) & 0xFF)
// Given an arena id, return the handle w/ the bits set
#define IN_ARENA(handle, arena) (((handle) | ARENA_MASK) >> 48)
#define WITH_ARENA(aid, h) ((h) | HANDLE_MASK | (((uint64_t)(aid)) << 48))


static alaska_arena_t *arenas[MAX_ARENAS];

uint64_t now_ns() {
  struct timespec spec;
  if (clock_gettime(1, &spec) == -1) { /* 1 is CLOCK_MONOTONIC */
    abort();
  }

  return spec.tv_sec * (1000 * 1000 * 1000) + spec.tv_nsec;
}


static void dump_arenas() {
  log("-------------------- ARENAS --------------------\n");
  for (int id = 0; id < MAX_ARENAS; id++) {
    if (arenas[id] == NULL) continue;

    alaska_arena_t *arena = arenas[id];
    log("aid:%d\n", arena->id);
    struct rb_node *node;
    for (node = rb_first(&arena->table); node; node = rb_next(node)) {
      alaska_handle_t *h = rb_entry(node, alaska_handle_t, node);
      log("   %p:%p -> %p | depth=%ld\n", h->handle, h->handle + h->size, h->backing_memory, h->pin_depth);
    }
  }
  log("\n");
}

void alaska_die(const char *msg) {
  fprintf(stderr, "alaska_die: %s\n", msg);
  abort();
}

static uint64_t pin_count = 0;
static uint64_t unpin_count = 0;
static uint64_t dyncall_count = 0;

static ALWAYS_INLINE alaska_handle_t *find_in_rbtree(uint64_t va, struct rb_root *handle_table) {
  // walk...
  struct rb_node **n = &(handle_table->rb_node);
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

// Given a handle, fill the out variables w/ the relevant
// structures and return 0 on success. Returns -errno otherwise
static ALWAYS_INLINE int find_arena_and_handle(uint64_t h, alaska_handle_t **o_handle, alaska_arena_t **o_arena) {
  if (h & ARENA_MASK != ARENA_MASK) return -EINVAL;
  uint8_t aid = HANDLE_ARENA(h);
  // log("find(0x%llx): aid=%d\n", h, aid);

  *o_arena = arenas[aid];
  if (*o_arena == NULL) {
    log("No arena found!\n");
    return -ENOENT;
  }

  *o_handle = find_in_rbtree(h, &arenas[aid]->table);
  if (*o_handle == NULL) {
    log("No handle found!\n");
    return -ENOENT;
  }
  return 0;
}



void *alaska_arena_alloc(size_t sz, alaska_arena_t *arena) {
  // Calloc might be a waste here, as we initialize everything immediately
  alaska_handle_t *handle = calloc(1, sizeof(alaska_handle_t));
  // ask the arena's allocator to do the dirty work
  handle->backing_memory = arena->alloc(arena, sz);
  // assign a handle
  handle->handle = (uint64_t)WITH_ARENA(arena->id, arena->next_handle);
  handle->size = sz;
  handle->pin_depth = 0;
  arena->next_handle += round_up(16, sz);

  // insert it into the tree
  rb_insert(&arena->table, &handle->node, __insert_callback, (void *)handle);


  log("alloc(aid=%d) %p\n", arena->id, (void *)handle->handle);
  return (void *)handle->handle;
}


void *alaska_alloc(size_t sz) {
  return alaska_arena_alloc(sz, arenas[0]);
}

void alaska_free(void *ptr) {
  uint64_t h = (uint64_t)ptr;
  alaska_handle_t *handle;
  alaska_arena_t *arena;
  log("free  %p\n", ptr);
  if (find_arena_and_handle((uint64_t)ptr, &handle, &arena) != 0) {
    alaska_die("Failed to unpin!");
  }

  if (handle->pin_depth != 0) {
    alaska_die("Pin depth is not 0 when freeing. UAF likely. Bailing!\n");
  }
  arena->free(arena, handle->backing_memory);
  rb_erase(&handle->node, &arena->table);
  free(handle);
  // dump_arenas();
}




////////////////////////////////////////////////////////////////////////////
void *alaska_pin(void *ptr) {
	dyncall_count++;
  uint64_t h = (uint64_t)ptr;
  if ((h & HANDLE_MASK) != 0) {
    alaska_handle_t *handle;
    alaska_arena_t *arena;
  	log("  pin %p\n", ptr);
    if (find_arena_and_handle((uint64_t)ptr, &handle, &arena) != 0) {
      alaska_die("Failed to pin!");
      return NULL;
    }
    arena->pinned(arena, handle);
    handle->pin_depth++;
    // dump_arenas();
    pin_count++;
    return handle->backing_memory;
  }
  return ptr;
}


////////////////////////////////////////////////////////////////////////////
void alaska_unpin(void *ptr) {
  uint64_t h = (uint64_t)ptr;
  if ((h & HANDLE_MASK) != 0) {
    alaska_handle_t *handle;
    alaska_arena_t *arena;
    log("unpin %p\n", ptr);
    if (find_arena_and_handle((uint64_t)ptr, &handle, &arena) != 0) {
      alaska_die("Failed to unpin!");
    }

    --handle->pin_depth;
    arena->unpinned(arena, handle);
    unpin_count++;
    // dump_arenas();
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
  arena->table = RB_ROOT;
  arena->next_handle = 0x1000;
  return;
}

void *alaska_default_alloc(alaska_arena_t *arena, size_t sz) {
  // log("default_alloc\n");
  return malloc(sz);
}
void alaska_default_free(alaska_arena_t *arena, void *ptr) {
  // log("default_free\n");
  return free(ptr);
}
int alaska_default_pinned(alaska_arena_t *arena, alaska_handle_t *handle) {
  // log("default_pinned\n");
  return 0;
}

int alaska_default_unpinned(alaska_arena_t *arena, alaska_handle_t *handle) {
  // log("default_unpinned\n");
  return 0;
}

static void __attribute__((constructor)) alaska_init(void) {
  // Zero out the arenas array so invalid lookups don't cause UB
  for (int i = 0; i < MAX_ARENAS; i++) {
    arenas[i] = NULL;
  }

  alaska_arena_t *base_arena = calloc(1, sizeof(*base_arena));
  alaska_arena_init(base_arena);
}

static void __attribute__((destructor)) alaska_deinit(void) {
  log("=================================\n");
  log("ALASKA_PINS:          %llu\n", pin_count);
  log("ALASKA_UNPINS:        %llu\n", unpin_count);
  log("ALASKA_CALLS:         %llu\n", dyncall_count);
  // printf("ALASKA_US_PINNING: %llu\n", ns_pinning / 1000);
  // printf("ALASKA_MS_PINNING: %llu\n", ns_pinning / 1000 / 1000);
}
