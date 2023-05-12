#include <alaska.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>


struct node {
  struct node *left, *right;
  // char *data;
};

struct node *create_tree(int depth) {
  struct node *n = malloc(sizeof(*n));
	// printf("%p\n", n);
  // struct node *n = malloc(512);
  // const char *data = "Hello, world. This is a test of a big allocation of string";
  // n->data = malloc(strlen(data) + 1);
  // strcpy(n->data, data);
  // n->val = depth;
  // n->left = n->right = (void*)5;
  if (depth > 0) {
    n->left = create_tree(depth - 1);
    n->right = create_tree(depth - 1);
  }
  return n;
}

long num_nodes(struct node *tree) {
  if (tree == NULL) {
    return 0;
  }
  return 1 + num_nodes(tree->right) + num_nodes(tree->left);
}


long num_nodes_rev(struct node *tree) {
  if (tree == NULL) {
    return 0;
  }
  return 1L	+ num_nodes_rev(tree->right) + num_nodes_rev(tree->left);
}

void free_nodes(struct node *tree) {
  if (tree == NULL) return;

  free_nodes(tree->left);
  free_nodes(tree->right);
  free(tree);
}

void test(long (*func)(struct node *tree), struct node *tree, int barrier) {
  uint64_t start, end;
  start = alaska_timestamp();
  if (barrier) alaska_barrier();
  func(tree);
  end = alaska_timestamp();

  printf("%f\n", (end - start) / 1000.0 / 1000.0 / 1000.0);
}

int main() {
  long start, end;
  srand((unsigned)time(NULL));

  printf("=============== ALLOCATING ===============\n");
  start = alaska_timestamp();
  struct node *tree = create_tree(23);
  end = alaska_timestamp();
  printf("allocation took %fs\n", (end - start) / 1000.0 / 1000.0 / 1000.0);
	// return 0;


  long trials = 10;
  printf("Forwards:\n");
  for (int i = 0; i < trials; i++) {
    test(num_nodes, tree, 0);
  }

  printf("Reverse:\n");
  for (int i = 0; i < trials; i++) {
    test(num_nodes_rev, tree, 0);
  }

  // free_nodes(tree);
  return 0;
}
