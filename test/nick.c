#include "../src/rt/include/alaska.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
// #include "./miniz.c"

struct node {
  struct node *next;
  int val;
};

void *a(void) { return alaska_alloc(sizeof(struct node)); }
void *b(void) { return alaska_alloc(sizeof(struct node)); }
void *c(void) { return alaska_alloc(sizeof(struct node)); }

int main() {
  struct node *root = NULL;

	if (rand() == 1) {
		root = a();
	}
	if (rand() == 2) {
		root = b();
	}
	if (rand() == 3) {
		root = c();
	}

  root->next = NULL;
  root->val = rand();

  alaska_free(root);

  return 0;
}
