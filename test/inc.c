#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <alaska.h>
#include "../runtime/include/alaska/histogram.h"

#define managed __attribute__((address_space(1)))

extern void alaska_test_sm(void);


__attribute__((noinline)) volatile int *inc(volatile int *x) {
  int val = *x;
  // alaska_test_sm();
  *x = val + 1;

  return x;
}

// volatile int managed *slow_inc(volatile int managed *x) {
// 	volatile int *z = (volatile int *)x;
// 	int value = *z;
// 	inc(z);
// 	alaska_test_sm();
// 	*z = value + 1;
// 	return x;
// }

#define COUNT 10000000

// #pragma alaska
int main(int argc, char **argv) {
  Histogram h;
  histogram_clear(&h);


  int trials = 10;
  if (argc > 1) {
    trials = atoi(argv[1]);
  }

  volatile int *ptr = (volatile int *)malloc(sizeof(int));
  *ptr = 0;

  inc(ptr);
  alaska_test_sm();

  for (int j = 0; j < trials; j++) {
    uint64_t start = alaska_timestamp();
    *ptr = 0;

    int count = COUNT;  // rand() % COUNT;
    for (int i = 0; i < count; i++) {
      inc(ptr);
    }
    alaska_test_sm();


    double time = (double)(alaska_timestamp() - start);
    time /= (double)COUNT;
    printf("%lf\n", time);
    histogram_add(&h, time * 1000);
  }

  printf("Measured in picoseconds:\n");
  printf("Median: %f\n", histogram_median(&h));
  printf("Average: %f\n", histogram_average(&h));
  printf("StdDev: %f\n", histogram_stddev(&h));
  printf("90th: %f\n", histogram_percentile(&h, 0.90));
  printf("95th: %f\n", histogram_percentile(&h, 0.95));
  printf("99th: %f\n", histogram_percentile(&h, 0.99));

  histogram_print(&h, stdout);


  return 0;
}
