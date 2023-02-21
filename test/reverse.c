#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <alaska.h>
#define LENGTH 10

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

int main(int argc, char** argv) {
  struct node* cur = NULL;
  struct node* root = NULL;
  for (int i = 0; i < LENGTH; i++) {
    struct node* n = (struct node*)malloc(NODE_SIZE);
    n->next = root;
    n->val = i;
    root = n;
  }

  root = reverse_list(root);
  alaska_barrier();

  int sum = 0;
  cur = root;
  while (cur != NULL) {
    sum += cur->val;
    cur = cur->next;
  }

  printf("sum = %d\n", sum);
  alaska_barrier();
  //
  cur = root;
  while (cur != NULL) {
    struct node* prev = cur;
    cur = cur->next;
    if (prev->val & 2) {
      free(prev);
    }
  }
  alaska_barrier();
  return 0;
}
