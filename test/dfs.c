#include <alaska.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>


volatile int g_count = 0;
struct node {
  struct node *left, *right;
  char *data;
};


struct node *create_tree(int depth) {
  struct node *n = malloc(sizeof(*n));
  // struct node *n = malloc(512);
  const char *data = "Hello, world. This is a test of a big allocation of string";
  n->data = malloc(strlen(data) + 1);
  strcpy(n->data, data);
  // n->val = depth;
  n->left = n->right = NULL;
  if (depth > 0) {
    n->right = create_tree(depth - 1);
    n->left = create_tree(depth - 1);
  }
  return n;
}

long num_nodes(struct node *tree) {
  g_count++;
  if (tree == NULL) {
    return 0;
  }
  int count = 1;
  count += num_nodes(tree->left);
  count += num_nodes(tree->right);
  return count;
}


long num_nodes_rev(struct node *tree) {
  g_count++;
  if (tree == NULL) {
    return 0;
  }
  return 1 + num_nodes_rev(tree->right) + num_nodes_rev(tree->left);
}


long num_lefts(struct node *tree) {
  if (tree == NULL) {
    return 0;
  }
  return 1 + num_nodes(tree->left);
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


void test(long (*func)(struct node *tree), struct node *tree) {
  uint64_t start;
  // alaska_barrier();
  start = alaska_timestamp();
  func(tree);
  printf("%f\n", (alaska_timestamp() - start) / 1000.0 / 1000.0 / 1000.0);
}

int main() {
  srand((unsigned)time(NULL));

  printf("=============== ALLOCATING ===============\n");
  struct node *tree = create_tree(15);
  printf("=============== TRAVERSING ===============\n");
  num_nodes(tree);


  long trials = 10;
  printf("Before barrier:\n");
  for (int i = 0; i < trials; i++) {
    test(num_nodes_rev, tree);
  }
  long start = alaska_timestamp();
  alaska_barrier();
  long end = alaska_timestamp();
  printf("barrier took %fs\n", (end - start) / 1000.0 / 1000.0 / 1000.0);


  // printf("After barrier:\n");
  for (int i = 0; i < trials; i++) {
    test(num_nodes_rev, tree);
  }
	return 0;
}
