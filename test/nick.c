#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
// #include "./miniz.c"

struct node {
  struct node *next;
  int val;
};

int sum(struct node *root) {
  int sum = 0;
  struct node *cur = root;
  while (cur != NULL) {
    sum += cur->val;
    cur = cur->next;
  }
	return sum;
}

int main() {

  return 0;
}
