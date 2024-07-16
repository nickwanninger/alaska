#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

int main() {
  void *x = malloc(0);
  printf("x = %zx\n", (uintptr_t)x);

  x = realloc(x, 64);
  printf("x = %zx\n", (uintptr_t)x);
  return 0;
}
