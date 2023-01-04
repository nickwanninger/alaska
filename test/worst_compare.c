#include <alaska.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

uint64_t timestamp() {
#if defined(__x86_64__)
  uint32_t rdtsc_hi_, rdtsc_lo_;
  __asm__ volatile("rdtsc" : "=a"(rdtsc_lo_), "=d"(rdtsc_hi_));
  return (uint64_t)rdtsc_hi_ << 32 | rdtsc_lo_;
#else

  struct timespec spec;
  clock_gettime(1, &spec);
  return spec.tv_sec * (1000 * 1000 * 1000) + spec.tv_nsec;
#endif
}


#define TRIALS 1000
#define LENGTH 10000

struct node {
  struct node *next;
  int val;
};
#define NODE_SIZE sizeof(struct node)


struct node *reverse_list(struct node *root) {
  // Initialize current, previous and next pointers
  struct node *current = root;
  struct node *prev = NULL, *next = NULL;

  while (current != NULL) {
    // Store next
    next = current->next;
    // Reverse current node's pointer
    current->next = prev;
    // Move pointers one position ahead.
    prev = current;
    current = next;
  }
  return prev;
}

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
    struct node *n = (struct node *)_malloc(NODE_SIZE);
    n->next = root;
    n->val = i;
    root = n;
  }

  // Hacky way to "manufacture locality" without a fancy runtime.
  // Ideally, we'd see the alaska runtime do this for us, but that
  // part isn't written yet so this is a proxy for what that part
  // can do.
  // if (_malloc == halloc) root = reverse_list(root);

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
  uint64_t *baseline = run_test(malloc, free);
  uint64_t *alaska = run_test(halloc, hfree);


  float slowdowns[TRIALS];
  float sum_slowdowns = 0.0f;
  for (int i = 0; i < TRIALS; i++) {
    uint64_t a = alaska[i];
    uint64_t b = baseline[i];
    float slowdown = (float)a / (float)b;
    slowdowns[i] = slowdown;
    sum_slowdowns += slowdown;
    printf("%lu,%lu\n", b, a);
  }


  // Compute standard deviation
  float mean = sum_slowdowns / (float)TRIALS;
  float stddev = 0.0;
  for (int i = 0; i < TRIALS; i++) {
    stddev += pow(slowdowns[i] - mean, 2);
  }
  stddev = sqrt(stddev / (float)TRIALS);


  fprintf(stderr, "µ:%.3fx σ:%.2f\n", mean, stddev);

  free(alaska);
  free(baseline);

  return 0;
}