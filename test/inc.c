#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <alaska.h>

__attribute__((noinline)) void inc(volatile int *x) {
  (*x) += 1;
}

#define COUNT 10000000

// #pragma alaska
int main(int argc, char **argv) {

	int trials = 10;
	if (argc > 1) {
		trials = atoi(argv[1]);
	}

  // for (int i = 0; i < 16; i++) {
  volatile int *ptr = (volatile int *)halloc(sizeof(int));
  *ptr = 0;

  for (int j = 0; j < trials; j++) {
    uint64_t start = alaska_timestamp();
		*ptr = 0;
    for (int i = 0; i < COUNT; i++) {
      inc(ptr);
    }

    printf("%lf ns per inc (%d)\n", (alaska_timestamp() - start) / (double)COUNT, *ptr);
  }

  return 0;
}
