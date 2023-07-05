#include <alaska.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>


struct node {
  struct node *left, *right;
};

struct node *create_tree(int depth) {
	if (depth == 0) return NULL;
  struct node *n = malloc(sizeof(*n));
	n->left = create_tree(depth - 1);
	n->right = create_tree(depth - 1);
  return n;
}

long num_nodes(struct node *tree) {
  if (tree == NULL) {
    return 0;
  }
  return 1 + num_nodes(tree->right) + num_nodes(tree->left);
}

int main() {
  long start, end;
  start = alaska_timestamp();
  struct node *tree = create_tree(27);
  end = alaska_timestamp();
  printf("allocation took %fs\n", (end - start) / 1000.0 / 1000.0 / 1000.0);
  long trials = 20;
  for (int i = 0; i < trials; i++) {
		start = alaska_timestamp();
		long out = num_nodes(tree);
		end = alaska_timestamp();
		printf("%f\n", (end - start) / 1000.0 / 1000.0 / 1000.0);
  }
  return 0;
}
