#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <alaska.h>

__attribute__((noinline)) void write_ptr(volatile int *x) {
  *x = 1;
}

// #pragma alaska
int main(int argc, char **argv) {
  return 0;
}
