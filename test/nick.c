#include "../src/rt/include/alaska.h"
#include <stdio.h>
#include <stdlib.h>

struct foo {
  int x;
  int y;
};

volatile struct foo *global;
int main() {

  volatile struct foo *x = alaska_alloc(sizeof(*x));
  printf("x: %p\n", x);

  x->x = 0;

  // x->y = 1;
  // global = x;
  // printf("global: %p\n", global);

  // x = global;
  return 0;
}
