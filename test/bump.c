#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <alaska.h>

extern void *alaska_lock(void *);
extern void *alaska_unlock(void *);


typedef enum { FREE, ALLOC, LOCKED } block_type_t;
static void setup_heap(const char *input) {
  void *to_free[strlen(input)];
  int free_ind = 0;

  size_t len = 0;
  block_type_t type = FREE;

  while (*input) {
    switch (*input) {
      case '|':
      case '\0':
        // terminate
        if (len > 0) {
          // do the thing.
          printf("malloc %zu\n", len);
          void *ptr = malloc(len * 16);
          if (type == FREE) {
            // track that.
            to_free[free_ind++] = ptr;
          }
          if (type == LOCKED) {
            alaska_lock(ptr);
          }
        }
        len = 0;
        break;
      case '-':
        type = FREE;
        len++;
        break;
      case '#':
        type = ALLOC;
        len++;
        break;
      case 'X':
        type = LOCKED;
        len++;
        break;
    }
    input += 1;
  }

  for (int i = 0; i < free_ind; i++) {
    free(to_free[i]);
  }
}


void touch(volatile void *x) {
  *(volatile char *)x = 0;  // touch
}

void *bench_alloc(size_t sz) {
  void *x = malloc(sz);
  touch(x);
  return x;
}

void bench_free(void *ptr) {
  *(volatile char *)ptr = 0;  // touch
  free(ptr);
}



// this program is used in testing the anchorage bump allocator
int main(int argc, char **argv) {
  // setup_heap(
  //     "|------------------"
  //     "------------------------------------------|##|----|#########"
  //     "###|---------------------------------------------------"
  //     "------------------|##|-----------------"
  //     "---------------------"
  //     "----------------------|##|---------------------------------|##|---|--|---|---|----|------------|--|--|--|"
  //     "--|--|--|---|--|------|--|----|------------|--|--|--|--|--|--|--|--|----|------------------------|--|--|--|--|--"
  //     "|--|--|--|--|--|----|---|--|--|------|---|------------|----|------------|--|--|---|---|------------|--|--|---|--"
  //     "-|--|---|--|------------|--|----|------------------------|--|---------------------------------------------------"
  //     "-----------------------------------------------------------------------------|--|---|--|--|--|--|---|--|--|--|--"
  //     "--|------------------------------------------------|--|--|--|--|--|--|--|--|--|--|--|--|--|--|---|----|---------"
  //     "---------------|--|--|----|------------------------------------------------|--|--|--|--|--|--|--|--|---|--|--|--"
  //     "|--|--|--|--|--|--|--|--|--|---|--|--|---|---|----|---|---|-----------------------------------------------------"
  //     "-------------------------------------------|--|----|------------|--|---|--|---|---|------------------------|--|-"
  //     "---|------------------------------------------------|---|--|--|---|---|---|---|---|---|--|---|---|---|---|----|-"
  //     "--|----|---|--|---|---|----|--------|--|#|#|####|--|####|---|---|##|#|--|-|--|-|---|----|----");
  
	// void *slot = bench_alloc(48);
  // void *locked = bench_alloc(16);
  // bench_alloc(32);
  // bench_alloc(48);
  // bench_alloc(128);
  // alaska_lock(locked);
  //
  // bench_free(slot);
  // alaska_barrier();
  //
  // alaska_unlock(locked);
  // alaska_barrier();
  // return 0;

  int count = 16;
  if (argc == 2) count = atoi(argv[1]);
  void **ptrs = calloc(count, sizeof(void *));

  for (int i = 0; i < count; i++)
    ptrs[i] = bench_alloc(16);


  for (int i = 0; i < count - 1; i += 2) {
    bench_free(ptrs[i]);
    ptrs[i] = NULL;
  }

  for (int i = 0; i < count - 1; i += 2)
    ptrs[i] = bench_alloc(64);

  alaska_lock(ptrs[count - 1]);
  // alaska_barrier();
  // printf("UNLOCKING\n\n\n");
  // alaska_unlock(ptrs[count - 1]);
  alaska_barrier();

  alaska_unlock(ptrs[count - 1]);
  alaska_barrier();
  for (int i = 0; i < count; i++) {
    bench_free(ptrs[i]);
    ptrs[i] = NULL;
  }
  bench_free(ptrs);

  return 0;
}
