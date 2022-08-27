#include <alaska.h>
#include <alaska/set.h>
#include <alaska/rbtree.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

// #include <jemalloc/jemalloc.h>

#define GRN "\e[0;32m"
#define GRY "\e[0;90m"
#define RESET "\e[0m"
#define PFX GRN "<alaska> " RESET

#ifdef ALASKA_DEBUG
#  define log(fmt, args...) printf(PFX fmt RESET, ##args)
#else
#  define log(...)
#endif

#define round_up(x, y) (((x) + (y)-1) & ~((y)-1))

uint64_t now_ns() {
  struct timespec spec;
  if (clock_gettime(1, &spec) == -1) { /* 1 is CLOCK_MONOTONIC */
    abort();
  }

  return spec.tv_sec * (1000 * 1000 * 1000) + spec.tv_nsec;
}

static uint64_t dynamic_calls = 0;
static uint64_t total_translations = 0;
static uint64_t ns_pinning = 0;

typedef struct {
  struct rb_node node; // the link to the rbtree
  uint64_t handle;     // the handle of this allocation
  uint64_t size;       // the size of this allocation
  uint8_t data;
} handle_t;

static struct rb_root handle_table;
static uint64_t next_handle = 0x1000;

#define HANDLE_MASK (0xFFFFLU << 48)


static handle_t *alaska_find(uint64_t va) {
  // walk...
  struct rb_node **n = &(handle_table.rb_node);
  struct rb_node *parent = NULL;

  int steps = 0;

  /* Figure out where to put new node */
  while (*n != NULL) {
    handle_t *r = rb_entry(*n, handle_t, node);

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
  handle_t *allocation = arg;
  handle_t *other = rb_entry(n, handle_t, node);
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
	void *ptr = malloc(sz);
	log("alloc %p\n", ptr);
	return ptr;

  handle_t *handle = malloc(sizeof(handle_t) + sz);
  handle->size = sz;
  handle->handle = (uint64_t)next_handle | HANDLE_MASK;
  next_handle += round_up(16, sz);

  rb_insert(&handle_table, &handle->node, __insert_callback, (void *)handle);
  log("alloc %p\n", (void *)handle->handle);
  // log("allocated: %p, %zu\n", (void *)handle->handle, sz);
  return (void *)handle->handle;
}



void alaska_free(void *ptr) {
	free(ptr);
	return;

  // return;
  uint64_t handle = (uint64_t)ptr;

  log("free request %p\n", ptr);
  return;
  if ((handle & HANDLE_MASK) != 0) {
    // uint64_t start = now_ns();
    handle_t *h = alaska_find(handle);
    if (h == NULL)
      return;
    total_translations += 1;
    log("free  %p -> %p\n", ptr, &h->data);
  }
}

void *alaska_pin(void *ptr) {
  dynamic_calls += 1;
  log("pin   %p\n", ptr);

	return ptr;

  uint64_t handle = (uint64_t)ptr;

  if ((handle & HANDLE_MASK) != 0) {
    log("pin   %p\n", ptr);
    handle_t *h = alaska_find(handle);
    total_translations += 1;
    log("translate %016lx -> %016lx\n", ptr, &h->data);
    return (void *)&h->data;
  }

  return (void *)handle;
}

void alaska_unpin(void *ptr) {
  log("unpin %p\n", ptr);
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
  printf("ALASKA_DYNAMIC_CALLS: %llu\n", dynamic_calls);
  printf("ALASKA_TRANSLATIONS:  %llu\n", total_translations);
  printf("ALASKA_NS_PINNING: %llu\n", ns_pinning);
  // printf("ALASKA_US_PINNING: %llu\n", ns_pinning / 1000);
  // printf("ALASKA_MS_PINNING: %llu\n", ns_pinning / 1000 / 1000);
}
