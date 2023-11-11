#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <alaska.h>
#include "../runtime/include/alaska/histogram.h"

#define managed __attribute__((address_space(1)))
#define COUNT 10000000

__attribute__((noinline)) static volatile int *inc(volatile int *x) {
  *x += 1;
  return x;
}

int main(int argc, char **argv) {

  printf("Hello!\n");
  // int a = 0;
  int *p = malloc(sizeof(*p));
  inc(p);
  printf("%p\n", p);
  // Histogram h;
  // histogram_clear(&h);
  //
  //
  // int trials = 10000;
  // if (argc > 1) {
  //   trials = atoi(argv[1]);
  // }
  //
  volatile int *ptr = (volatile int *)malloc(sizeof(int));
  *ptr = 0;

  inc(ptr);
  //
  // for (int j = 0; j < trials; j++) {
  //   uint64_t start = alaska_timestamp();
  //   *ptr = 0;
  //
  //   int count = COUNT;  // rand() % COUNT;
  //   for (int i = 0; i < count; i++) {
  //     inc(ptr);
  //   }
  //
  //
  //   double time = (double)(alaska_timestamp() - start);
  //   time /= (double)COUNT;
  //   printf("%lf\n", time);
  //   histogram_add(&h, time * 1000);
  // }
  //
  // printf("Measured in picoseconds:\n");
  // printf("Median: %f\n", histogram_median(&h));
  // printf("Average: %f\n", histogram_average(&h));
  // printf("StdDev: %f\n", histogram_stddev(&h));
  // printf("90th: %f\n", histogram_percentile(&h, 0.90));
  // printf("95th: %f\n", histogram_percentile(&h, 0.95));
  // printf("99th: %f\n", histogram_percentile(&h, 0.99));
  //
  // histogram_print(&h, stdout);
  //

  return 0;
}
