#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <alaska.h>

extern void *alaska_lock(void *);
extern void *alaska_unlock(void *);


#define ATTR __attribute__((noinline))
// #define ATTR inline

ATTR void touch(volatile void *x) {
  *(volatile char *)x = 0;  // touch
  // printf("Touch %p\n", x);
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
  int stride = 3;
  // printf("stride %d\n", stride);
  void **array = calloc(count, sizeof(void *));
  long start = alaska_timestamp();

  for (int i = 0; i < count; i++) {
    array[i] = bench_alloc(1);
  }

  for (int j = 0; j < 1; j++) {
    for (int i = 1; i < count; i += stride) {
      free(array[i]);
    }

    for (int i = 1; i < count; i += stride) {
      array[i] = bench_alloc(2 + j);
    }

    alaska_barrier();
		printf("\n\n");
  }


  // for (int i = 1; i < count; i += stride) {
  //   array[i] = bench_alloc(3);
  // }
  //
  //
  // alaska_barrier();
  long end = alaska_timestamp();


  // printf("%d, %f\n", count, (end - start) / 1024.0 / 1024.0);
  printf("%d,%f\n", count, (end - start) / 1024.0 / 1024.0);

  for (int i = 0; i < count; i += 1) {
    free(array[i]);
  }
  free(array);
}

// this program is used in testing the anchorage bump allocator
int main(int argc, char **argv) {

	// for (size_t i = 16; i < 2048 * 1024; i *= 2) {
	// 	void *x = malloc(i);
	// 	printf("x = %p\n", x);
	// 	free(x);
	// }
	// return 0;

  run_test(24);
  return 0;

  // for (int count = 16; count <= 1024; count += 16) {
  //
  // // printf("request %d\n", count);
  //   volatile void *x = malloc(count);
  //   printf("x=%d %p\n", count, (uint64_t)x);
  //
  // void *other = malloc(32);
  // printf("other = %p\n", (uint64_t)other);
  // // printf("\nFREE\n");
  //   free((void*)x);
  // free(other);
  // // printf("\n\n\n\n");
  // }
  // return 0;
  // void *x = bench_alloc(2);
  // void *y = bench_alloc(4);
  // bench_alloc(6);
  // bench_alloc(2);
  // bench_free(x);
  // alaska_barrier();
  // return 0;


  run_test(24);
  return 0;

  int max = 160000;


  for (int count = 8; count <= max; count *= 1.25) {
    run_test(count);
  }

  return 0;
}
