#include "../src/rt/include/alaska.h"
#include <stdio.h>
#include <stdlib.h>

#include "./miniz.c"


struct foo {
  int x;
  int y;
};

alaska_arena_t test_arena;


alaska_rt int invert(alaska_arena_t *arena, alaska_handle_t *handle) {
	uint8_t *p = handle->backing_memory;
	for (size_t i = 0; i < handle->size; i++) p[i] = ~p[i];
  return 0;
}

int main() {
  alaska_arena_init(&test_arena);
  test_arena.pinned = invert;
  test_arena.unpinned = invert;

  volatile struct foo *x = alaska_arena_alloc(sizeof(*x), &test_arena);
  x->x = 42;
	printf("val: %d\n", x->x);
  alaska_free((void *)x);
  return 0;
}
