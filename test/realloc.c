#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

#define HUGE_SIZE (1LU << 32)

int main() {
  int *x = (int *)malloc(8);

  printf("x = %zx\n", (uintptr_t)x);
  *x = 42;

  x = (int *)realloc(x, 64);
  printf("x = %zx\n", (uintptr_t)x);

  x = (int *)realloc(x, HUGE_SIZE);
  printf("x = %zx\n", (uintptr_t)x);

  x = (int *)realloc(x, 32);
  printf("x = %zx\n", (uintptr_t)x);

  return 0;
}
