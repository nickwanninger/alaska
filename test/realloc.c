#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

int main() {
  int *x = (int *)malloc(8);

  printf("x = %zx\n", (uintptr_t)x);
  *x = 42;

  x = (int *)realloc(x, 64);
  printf("x = %zx\n", (uintptr_t)x);

  *x += 1;
  return 0;
}
