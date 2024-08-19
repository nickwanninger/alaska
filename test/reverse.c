#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>



struct node {
  struct node* next;
  int val;
};

struct node *next_bump = NULL;
static void initialize_bump(int count) {
  next_bump = calloc(count, sizeof(struct node));
}

struct node* allocate_node() {
  return next_bump++;

  struct node* n = (struct node*)malloc(sizeof(struct node));

  return n;
}

struct node* reverse_list(struct node* root) {
  // Initialize current, previous and next pointers
  struct node* current = root;
  struct node *prev = NULL, *next = NULL;

  int i = 0;
  while (current != NULL) {
    // Store next
    next = current->next;
    // Reverse current node's pointer
    current->next = prev;
    // Move pointers one position ahead.
    prev = current;
    current = next;
    i++;
  }
  return prev;
}

struct node* alloc_list(int length) {
  if (length == 0) return NULL;
  struct node* n = allocate_node();
  n->next = alloc_list(length - 1);
  n->val = length;
  return n;
}

uint64_t timestamp() {
  uint32_t rdtsc_hi_, rdtsc_lo_;
  __asm__ volatile("rdtsc" : "=a"(rdtsc_lo_), "=d"(rdtsc_hi_));
  return (uint64_t)rdtsc_hi_ << 32 | rdtsc_lo_;
}

int main(int argc, char** argv) {
  int trials = 3000;
  int count = 8192;
  if (argc > 1) {
    count = atoi(argv[1]);
  }
  initialize_bump(count);
  struct node* root = alloc_list(count);

  uint64_t sum = 0;
  for (int i = 0; i < trials; i++) {
    uint64_t start = timestamp();
    root = reverse_list(root);
    uint64_t end = timestamp();
    sum += (end - start);
  }

  printf("average cycles: %f\n", sum / (float)trials);
  return 0;
}
