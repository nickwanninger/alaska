#include <alaska.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>


volatile int g_count = 0;
struct node {
  struct node *left, *right;
};


struct node *create_tree(int depth) {
  struct node *n = malloc(sizeof(*n));
  n->left = n->right = NULL;
  if (depth > 0) {
    n->right = create_tree(depth - 1);
    n->left = create_tree(depth - 1);
  }
  return n;
}



int main() {
  long start = alaska_timestamp();
  (void)create_tree(22);
  long end = alaska_timestamp();
  printf("allocating tree took %fs\n", (end - start) / 1000.0 / 1000.0 / 1000.0);
	return 0;
}
