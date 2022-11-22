#include "../runtime/include/alaska.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>



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


#define TRIALS 100
#define LENGTH 100000

struct node {
  struct node *next;
  int val;
};


int test(struct node *root, uint64_t *out) {
  volatile int sum = 0;
  for (int i = 0; i < TRIALS; i++) {
    uint64_t start = timestamp();
    struct node *cur = root;
    while (cur != NULL) {
      sum += cur->val;
      cur = cur->next;
    }
    uint64_t end = timestamp();
    out[i] = (end - start);
  }
  return sum;
}


uint64_t *run_test(void *(*_malloc)(size_t), void (*_free)(void *)) {
  uint64_t *trials = (uint64_t *)calloc(TRIALS, sizeof(uint64_t));

  struct node *root = NULL;
  for (int i = 0; i < LENGTH; i++) {
    struct node *n = (struct node *)_malloc(sizeof(struct node));
    n->next = root;
    n->val = i;
    root = n;
  }

  test(root, trials);


  struct node *cur = root;
  cur = root;
  while (cur != NULL) {
    struct node *prev = cur;
    cur = cur->next;
    _free(prev);
  }
  return trials;
}

int main(int argc, char **argv) {
  printf("baseline,alaska\n");
  for (int i = 0; i < 100; i++) {
    uint64_t *baseline = run_test(malloc, free);
    uint64_t *alaska = run_test(alaska_alloc, alaska_free);


    for (int i = 0; i < TRIALS; i++) {
      uint64_t a = alaska[i];
      uint64_t b = baseline[i];
      printf("%lu,%lu\n", b, a);
    }

    free(alaska);
    free(baseline);
		// usleep(100000);
  }

  return 0;
}
