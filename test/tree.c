#include <stdio.h>
#include <stdlib.h>

typedef struct node {
  struct node *left, *right;
  int value;
} node_t;


node_t *make_tree(int depth) {
  if (depth == 0) return NULL;

  node_t *n = calloc(1, sizeof(*n));

  n->left = make_tree(depth - 1);
  n->right = make_tree(depth - 1);
  n->value = rand();

  return n;
}


int tree_count_nodes(node_t *n) {
  if (n == NULL) return 0;
  return 1 + tree_count_nodes(n->left) + tree_count_nodes(n->right);
}

int main() {
  node_t *n = make_tree(23);


  while (1) {
    for (int i = 0; i < 512; i++) {
      int c = tree_count_nodes(n);
    }
    // printf("Node count: %d\n", c);
  }

  return EXIT_SUCCESS;
}
