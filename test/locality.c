#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <alaska.h>
#include <alaska/internal.h>

struct node {
  struct node *next;
  int val;
};

int sum_list(struct node *root) {
  int sum = 0;
  struct node *cur = root;
  while (cur != NULL) {
		printf("use %p\n", cur);
    sum += cur->val;
    cur = cur->next;
  }
  return sum;
}

int main(int argc, char **argv) {
  struct node *root = NULL;
  for (int i = 0; i < 8; i++) {
    struct node *n = (struct node *)halloc(sizeof(struct node));
    n->next = root;
    n->val = i;
    root = n;
  }

  printf("sum = %d\n", sum_list(root));
  alaska_barrier();
  printf("sum = %d\n", sum_list(root));

  return 0;
}
