#include "../runtime/include/alaska.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

struct node {
  struct node *next;
  int val;
};

int sum_list(struct node *root) {
  int sum = 0;
  struct node *cur = root;
  while (cur != NULL) {
    sum += cur->val;
    cur = cur->next;
  }
  return sum;
}

int main(int argc, char **argv) {
  struct node *root = NULL;
  for (int i = 0; i < 16; i++) {
    struct node *n = (struct node *)alaska_alloc(sizeof(struct node));
    n->next = root;
    n->val = i;
    root = n;
  }

  printf("sum = %d\n", sum_list(root));
  alaska_barrier();
  printf("sum = %d\n", sum_list(root));

  return 0;
}
