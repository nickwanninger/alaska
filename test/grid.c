#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

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


#define TRIALS 100

struct node {
  struct node *next;
  float data[];
};



float test(struct node *root, int nodes, int work) {
  volatile float sum = 0;
  uint64_t start = timestamp();
  for (int i = 0; i < TRIALS; i++) {
    struct node *cur = root;
    while (cur != NULL) {
      for (int w = 0; w < work; w++) {
        sum += cur->data[w];
      }
      cur = cur->next;
    }
  }
  uint64_t end = timestamp();
  return (end - start) / (float)TRIALS;
}


float run_test(int nodes, int work) {
  struct node *root = NULL;
  for (int i = 0; i < nodes; i++) {
    struct node *n = (struct node *)malloc(sizeof(struct node) + (work * sizeof(float)));
    n->next = root;
    root = n;
  }

  float time = test(root, nodes, work);


  struct node *cur = root;
  cur = root;
  while (cur != NULL) {
    struct node *prev = cur;
    cur = cur->next;
    free(prev);
  }
  return time;
}

int main() {
  printf("nodes,work,time\n");
  for (int nodes = 10; nodes < 10000; nodes *= 1.5) {
    for (int work = 10; work < 10000; work *= 1.5) {
      printf("%d,%d,%f\n", nodes, work, run_test(nodes, work));
    }
  }

  return 0;
}
