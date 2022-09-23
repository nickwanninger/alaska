#include "../src/rt/include/alaska.h"
#include <stdio.h>
#include <stdlib.h>


struct foo {
  int x;
  int y;
};


int main() {
  alaska_arena_t arena;

  alaska_arena_init(&arena);
  printf("Initialized to %d\n", arena.id);

  volatile struct foo *x = alaska_alloc(sizeof(*x));
  printf("x: %p\n", x);
  x->x = 0;
  alaska_free((void *)x);
  return 0;
}
