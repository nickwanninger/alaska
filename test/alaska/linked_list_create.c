#include "alaska.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct node {
  struct node *next;
  int val;
};

int main(int argc, char **argv) {
  int count = 10;
  if (argc > 2)
    count = atoi(argv[1]);

  struct node *root = NULL;
  for (int i = 0; i < count; i++) {
    struct node *n = (struct node *)alaska_alloc(sizeof(struct node));
    n->val = rand();
    n->next = root;
    root = n;
  }

  return 0;
}
