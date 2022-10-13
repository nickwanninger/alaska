#include "../src/rt/include/alaska.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
// #include "./miniz.c"

struct node {
  struct node *next;
  int val;
};

int main() {
  struct node *root = NULL;
  root = (struct node *)alaska_alloc(sizeof(struct node));

  root->next = NULL;
  root->val = rand();

  alaska_free(root);

  return 0;
}
