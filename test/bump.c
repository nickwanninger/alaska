#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <alaska.h>

extern void *alaska_lock(void *);
extern void *alaska_unlock(void *);



void touch(volatile void *x) {
  *(volatile char *)x = 0;  // touch
}

void *bench_alloc(size_t sz) {
  void *x = calloc(1, sz);
  touch(x);
  return x;
}

void bench_free(void *ptr) {
  *(volatile char *)ptr = 0;  // touch
  free(ptr);
}

// this program is used in testing the anchorage bump allocator
int main(int argc, char **argv) {
  int count = 16;
  if (argc == 2) count = atoi(argv[1]);
  void **ptrs = calloc(count, sizeof(void *));

  for (int i = 0; i < count; i++)
    ptrs[i] = bench_alloc(64);


  for (int i = 0; i < count - 1; i += 2) {
    bench_free(ptrs[i]);
    ptrs[i] = NULL;
  }

  for (int i = 0; i < count - 1; i += 2) {
    ptrs[i] = bench_alloc(16);
  }

  // anchorage_manufacture_locality((void *)ptrs);

  // printf("=============== BARRIER ===============\n");
  alaska_barrier();
  // printf("=============== FREEING ===============\n");

  for (int i = 0; i < count; i++) {
    bench_free(ptrs[i]);
    ptrs[i] = NULL;
  }
  bench_free(ptrs);

  return 0;
}
