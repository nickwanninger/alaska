#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

struct node {
  struct node* next;
  int val;
};

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
  struct node* n = (struct node*)malloc(sizeof(struct node));
  n->next = alloc_list(length - 1);
  n->val = length;
  return n;
}

uint64_t timestamp() {
  uint32_t rdtsc_hi_, rdtsc_lo_;
  __asm__ volatile("rdtsc" : "=a"(rdtsc_lo_), "=d"(rdtsc_hi_));
  return (uint64_t)rdtsc_hi_ << 32 | rdtsc_lo_;
}

int main(int argc, char **argv) {
  struct node* root = alloc_list(8192);

  uint64_t sum = 0;
  int count = atoi(argv[1]);
  for (int i = 0; i < count; i++) {
    uint64_t start = timestamp();
    root = reverse_list(root);
    uint64_t end = timestamp();
    sum += (end - start);
  }

  printf("average cycles: %f\n", sum / (float)count);
  return 0;
}
