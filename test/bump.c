#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <alaska.h>

extern void *alaska_lock(void *);
extern void *alaska_unlock(void *);


// #define ATTR __attribute__((noinline))
#define ATTR inline

ATTR void touch(volatile void *x) {
  *(volatile char *)x = 0;  // touch
}

ATTR void *bench_alloc(size_t block_count) {
  void *x = calloc(block_count, 16);
  touch(x);
  return x;
}

ATTR void bench_free(void *ptr) {
  *(volatile char *)ptr = 0;  // touch
  free(ptr);
}

void run_test(int count) {

	int stride = 2;
	// printf("stride %d\n", stride);
	void **array = calloc(count, sizeof(void*));
  // void *array[count];

  for (int i = 0; i < count; i++) {
    array[i] = bench_alloc(2);
  }

  for (int i = 1; i < count; i += stride) {
    free(array[i]);
  }

  for (int i = 1; i < count; i += stride) {
    array[i] = bench_alloc(4);
  }

  long start = alaska_timestamp();
  alaska_barrier();
  long end = alaska_timestamp();

  for (int i = 1; i < count; i += stride) {
    free(array[i]);
  }

  alaska_barrier();

  for (int i = 1; i < count; i += stride) {
    array[i] = bench_alloc(6);
  }

  // printf("%d, %f\n", count, (end - start) / 1024.0 / 1024.0);
  printf("%f\n", (end - start) / 1024.0 / 1024.0);


  for (int i = 0; i < count; i += 1) {
    free(array[i]);
  }
	free(array);
}

// this program is used in testing the anchorage bump allocator
int main(int argc, char **argv) {
  // void *x = bench_alloc(2);
  // void *y = bench_alloc(4);
  // bench_alloc(6);
  // bench_alloc(2);
  // bench_free(x);
  // alaska_barrier();
  // return 0;


  run_test(32);
  return 0;

  int max = 16000;


  for (int count = 8; count <= max; count *= 1.25) {
    run_test(count);
  }

  return 0;
}
