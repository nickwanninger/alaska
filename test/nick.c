#include "alaska.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct node {
  struct node *next;
  int val;
};

// #define _malloc alaska_alloc
// #define _free alaska_free

#define _malloc malloc
#define _free free

static uint64_t now_ns() {
  struct timespec spec;
  if (clock_gettime(1, &spec) == -1) { /* 1 is CLOCK_MONOTONIC */
    abort();
  }

  return spec.tv_sec * (1000 * 1000 * 1000) + spec.tv_nsec;
}


int test(struct node *root) {
  volatile int sum = 0;

  float times[100];
  for (int i = 0; i < 100; i++) {
    uint64_t start = now_ns();
    struct node *cur = root;
    while (cur != NULL) {
      sum += cur->val;
      cur = cur->next;
    }

    uint64_t end = now_ns();
    times[i] = (end - start) / 1000.0;
  }
  for (int i = 0; i < 100; i++) {
    printf("%f\n", times[i]);
  }
  return sum;
}

int main(int argc, char **argv) {
  srand(0);
  struct node *root = NULL;
  // Allocate then free a linked list
  int count = 100000;

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
