#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <alaska.h>

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

#define LENGTH 100000

struct node {
  struct node* next;
  int val;
};
#define NODE_SIZE sizeof(struct node)

struct node* reverse_list(struct node* root) {
  // Initialize current, previous and next pointers
  struct node* current = root;
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

int test(volatile struct node* root) {
  volatile int sum = 0;
  volatile struct node* cur = root;
  while (cur != NULL) {
    sum += cur->val;
    cur = cur->next;
  }
  return sum;
}

uint64_t run_test(void) {
  struct node* root = NULL;
  for (int i = 0; i < LENGTH; i++) {
    struct node* n = (struct node*)malloc(NODE_SIZE);
    n->next = root;
    n->val = i;
    root = n;
  }

	root = reverse_list(root);
  test(root);
#ifdef ALASKA_SERVICE_ANCHORAGE
  anchorage_manufacture_locality((void*)root);
#endif

  uint64_t start = timestamp();
	for (int i = 0; i < 100; i++) {
  	test(root);
	}


  uint64_t end = timestamp();

  struct node* cur = root;
  cur = root;
  while (cur != NULL) {
    struct node* prev = cur;
    cur = cur->next;
    free(prev);
  }

  return end - start;
}

int main(int argc, char** argv) {
	for (int i = 0; i < 10; i++) {
  	uint64_t res = run_test();
    printf("%lu\n", res);
	}
  return 0;
}
