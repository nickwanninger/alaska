#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>


__attribute__((noinline)) static volatile int *inc(volatile int *x) {
  *x += 1;
  return x;
}

int main(int argc, char **argv) {
  for (int i = 0; i < 10; i++) {
    int *p = malloc(sizeof(*p));
    *p = 4;
    printf("0x%lx\n", (uintptr_t)p);
    inc(p);
  }
  return 0;
}
