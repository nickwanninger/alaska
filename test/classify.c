#include <alaska.h>
#include <stdlib.h>
#include <stdio.h>



struct node {
  struct node *left, *right;
};


struct node *create_tree(int depth) {
  struct node *n = malloc(sizeof(*n));
  alaska_classify(n, depth);
  if (depth > 0) {
    n->left = create_tree(depth - 1);
    n->right = create_tree(depth - 1);
  } else {
    n->left = n->right = NULL;
  }

  return n;
}

long num_nodes(struct node *tree) {
  if (tree == NULL) {
    return 0;
  }
  return 1 + num_nodes(tree->left) + num_nodes(tree->right);
}

int main() {
  struct node *tree = create_tree(10);

  long nodes = num_nodes(tree);
	printf("nodes: %ld\n", nodes);
}
