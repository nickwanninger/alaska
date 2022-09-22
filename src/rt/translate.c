#include <alaska.h>
#include <alaska/rbtree.h>
#include <alaska/set.h>
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
  struct rb_node node; // the link to the rbtree
  uint64_t handle;     // the handle of this allocation
  uint64_t size;       // the size of this allocation
  uint8_t data;
};

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

#define HANDLE_MASK (0xFFFFLU << 48)

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
  // log("allocated: %p, %zu\n", (void *)handle->handle, sz);
  return (void *)handle->handle;
}

void alaska_free(void *ptr) {
  uint64_t handle = (uint64_t)ptr;

  if ((handle & HANDLE_MASK) != 0) {
    // uint64_t start = now_ns();
    alaska_handle_t *h = alaska_find(handle);
    if (h == NULL)
      return;
    log("free  %p -> %p\n", ptr, &h->data);
  }
}

void *alaska_pin(void *ptr) {

  uint64_t handle = (uint64_t)ptr;
  if ((handle & HANDLE_MASK) != 0) {
    alaska_handle_t *h = alaska_find(handle);
    if (h == NULL)
      alaska_die("pin non handle");
    pin_count += 1;
    log("pin   %016lx -> %016lx\n", ptr, &h->data);
    return (void *)&h->data;
  }

  return (void *)handle;
}

void alaska_unpin(void *ptr) {
  log("unpin %p\n", ptr);

  uint64_t handle = (uint64_t)ptr;
  if ((handle & HANDLE_MASK) != 0) {
    alaska_handle_t *h = alaska_find(handle);
    if (h == NULL)
      alaska_die("unpin non handle");
		unpin_count += 1;
  }
}

void alaska_barrier(void) {
  // Does nothing for now. This function acts like a compiler intrinsic
  // and holds the invariant that all pins that a function is using have
  // been unpinned before calling this function. They will be re-pinned
  // after returning.
  //
  // The point of this function is to allow the runtime to repack memory
  // or whatever it wants to do during funciton execution.
  log("--- barrier ---\n");
}

static void __attribute__((constructor)) alaska_init(void) {
  handle_table = RB_ROOT;
}

static void __attribute__((destructor)) alaska_deinit(void) {
  log("=================================\n");
  log("ALASKA_PINS:          %llu\n", pin_count);
  log("ALASKA_UNPINS:        %llu\n", unpin_count);
  // printf("ALASKA_US_PINNING: %llu\n", ns_pinning / 1000);
  // printf("ALASKA_MS_PINNING: %llu\n", ns_pinning / 1000 / 1000);
}
