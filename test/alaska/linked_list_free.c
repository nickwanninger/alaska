#include "alaska.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct node {
  struct node *next;
  int val;
};

int main(int argc, char **argv) {
  srand(0);
  struct node *root = NULL;
  // Allocate then free a linked list
  int count = 10;

  for (int i = 0; i < count; i++) {
    struct node *n = (struct node *)halloc(sizeof(struct node));
    n->val = i;
    n->next = root;
    root = n;
  }

  int sum = 0;
  struct node *cur = root;
  while (cur != NULL) {
    sum += cur->val;
    struct node *prev = cur;
    cur = cur->next;
    hfree(prev);
  }

  printf("sum=%d\n", sum);

  return 0;
}
