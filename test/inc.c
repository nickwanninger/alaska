#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define managed __attribute__((address_space(1)))
#define COUNT 10000000

__attribute__((noinline)) static volatile int *inc(volatile int *x) {
  *x += 1;
  return x;
}

int main(int argc, char **argv) {
  for (int i = 0; i < 20; i++) {
    int *p = malloc(sizeof(*p));
    *p = 4;
    printf("handle = 0x%llx\n", (uintptr_t)p);
    inc(p);
    free(p);
  }
  return 0;
}
