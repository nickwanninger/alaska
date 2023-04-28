#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <alaska.h>

__attribute__((noinline)) void inc(volatile int *x) {
  (*x) += 1;
}

// #pragma alaska
int main(int argc, char **argv) {
  // for (int i = 0; i < 16; i++) {
  volatile int *ptr = (volatile int *)malloc(sizeof(int));
  *ptr = 0;

  printf("%d\n", *ptr);
  inc(ptr);
  printf("%d\n", *ptr);

  return 0;
}
