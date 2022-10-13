#include "alaska.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct node {
  struct node *next;
  int val;
};

int main(int argc, char **argv) {
  // Allocate then free a linked list
  int count = 10;
  if (argc > 2) count = atoi(argv[1]);

  struct node *root = NULL;
  for (int i = 0; i < count; i++) {
    struct node *n = (struct node *)alaska_alloc(sizeof(struct node));
    n->val = rand();
    n->next = root;
    root = n;
  }

  int sum = 0;
  struct node *cur = root;
  while (cur != NULL) {
    sum += cur->val;
    cur = cur->next;
  }

  printf("sum=%d\n", sum);

  return 0;
}
