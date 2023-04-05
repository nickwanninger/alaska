#include <alaska.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

struct node {
  struct node *left, *right;
};


struct node *create_tree(int depth) {
  struct node *n = malloc(sizeof(*n));
  n->left = n->right = NULL;
  if (depth > 0) {
    if (rand() % 1 == 0) n->left = create_tree(depth - 1);
    if (rand() % 1 == 0) n->right = create_tree(depth - 1);
  }

  return n;
}

long num_nodes(struct node *tree) {
  if (tree == NULL) {
    return 0;
  }
  return 1 + num_nodes(tree->left) + num_nodes(tree->right);
}


void free_nodes(struct node *tree) {
  if (tree == NULL) return;

  free_nodes(tree->left);
  free_nodes(tree->right);
  free(tree);
}


void free_lefts(struct node *tree) {
  if (tree == NULL) return;

  free_lefts(tree->left);
	free(tree->left);
  free_lefts(tree->right);
}

int main() {
  srand((unsigned)time(NULL));
  struct node *tree = create_tree(4);
  long nodes = num_nodes(tree);
  printf("nodes: %ld\n", nodes);
  free_lefts(tree);
	alaska_barrier();
}
