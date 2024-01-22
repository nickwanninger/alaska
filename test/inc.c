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
  for (int i = 0; i < 0x2000; i++) {
    int *p = malloc(sizeof(*p));
    *p = 4;
    printf("%zx\n", (uint64_t)p);
    inc(p);
  }
  return 0;
}
