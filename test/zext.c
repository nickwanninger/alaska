#include <stdlib.h>
#include <stdio.h>

#define noinline __attribute__((noinline))

noinline unsigned short square(unsigned short x) {
  return x * x;
}


noinline unsigned long pain(unsigned long x) {
  return square(x);
}

int main() {
  printf("%lx\n", pain(0x101073));
  return 0;
}