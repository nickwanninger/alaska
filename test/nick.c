#include "../runtime/include/alaska.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


struct node {
  struct node *next;
  int val;
};


uint64_t timestamp() {
#if defined(__x86_64__)
  uint32_t rdtsc_hi_, rdtsc_lo_;
  __asm__ volatile("rdtsc" : "=a"(rdtsc_lo_), "=d"(rdtsc_hi_));
  return (uint64_t)rdtsc_hi_ << 32 | rdtsc_lo_;
#else

  struct timespec spec;
  if (clock_gettime(1, &spec) == -1) { /* 1 is CLOCK_MONOTONIC */
    abort();
  }

  return spec.tv_sec * (1000 * 1000 * 1000) + spec.tv_nsec;
#endif
}


extern void do_nothing();

#define TRIALS 100

int test(struct node *root) {
  volatile int sum = 0;

  uint64_t times[TRIALS];
  for (int i = 0; i < TRIALS; i++) {
    uint64_t start = timestamp();
    struct node *cur = root;
    while (cur != NULL) {
      sum += cur->val;
      cur = cur->next;
    }

    uint64_t end = timestamp();
    times[i] = (end - start);
  }
  for (int i = 0; i < TRIALS; i++) {
    printf("%zu\n", times[i]);
  }
  return sum;
}

int main(int argc, char **argv) {
  void *(*_malloc)(size_t);
  void (*_free)(void *);

  if (argc == 2 && !strcmp(argv[1], "baseline")) {
    _malloc = malloc;
    _free = free;
  } else {
    _malloc = alaska_alloc;
    _free = alaska_free;
  }

  srand(0);
  struct node *root = NULL;
  int count = 1000;
  for (int i = 0; i < count; i++) {
    struct node *n = (struct node *)_malloc(sizeof(struct node));
    n->next = root;
    n->val = i;
    root = n;
  }

  test(root);

  return 0;

  struct node *cur = root;
  cur = root;
  while (cur != NULL) {
    struct node *prev = cur;
    cur = cur->next;
    _free(prev);
  }



  return 0;
}
