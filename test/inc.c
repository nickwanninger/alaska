#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <alaska.h>
#include "../runtime/include/alaska/histogram.h"


__attribute__((noinline)) void inc(volatile int *x) {
  (*x) += 1;
}

#define COUNT 10000000

// #pragma alaska
int main(int argc, char **argv) {
  Histogram h;
	histogram_clear(&h);

  int trials = 100;
  if (argc > 1) {
    trials = atoi(argv[1]);
  }

  volatile int *ptr = (volatile int *)halloc(sizeof(int));
  *ptr = 0;

  for (int j = 0; j < trials; j++) {
    uint64_t start = alaska_timestamp();
    *ptr = 0;

		int count = COUNT; // rand() % COUNT;
    for (int i = 0; i < count; i++) {
      inc(ptr);
    }


    double time = (double)(alaska_timestamp() - start);
    time /= (double)COUNT;
    printf("%lf ns per inc (%d)\n", time, *ptr);
    histogram_add(&h, time);
  }

  printf("Median: %f\n", histogram_median(&h));
  printf("Average: %f\n", histogram_average(&h));
  printf("StdDev: %f\n", histogram_stddev(&h));
  printf("90th: %f\n", histogram_percentile(&h, 0.90));
  printf("95th: %f\n", histogram_percentile(&h, 0.95));
  printf("99th: %f\n", histogram_percentile(&h, 0.99));

  histogram_print(&h, stdout);

  return 0;
}
