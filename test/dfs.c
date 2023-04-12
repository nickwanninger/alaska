#include <alaska.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>


volatile int count = 0;
struct node {
  struct node *left, *right;
  char *data;
  // int val;
};


struct node *create_tree(int depth) {
  struct node *n = malloc(sizeof(*n));
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
  count++;
  if (tree == NULL) {
    return 0;
  }
  int count = 1;
  count += num_nodes(tree->left);
  count += num_nodes(tree->right);
  return count;
}


long num_nodes_rev(struct node *tree) {
  count++;
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


int main() {
  srand((unsigned)time(NULL));

  printf("=============== ALLOCATING ===============\n");
  struct node *tree = create_tree(3);
  printf("=============== TRAVERSING ===============\n");
  num_nodes(tree);

  printf("================= BARRIER ================\n");
  alaska_barrier();
  // printf("=============== MANUFACTURING LOCALITY ===============\n");
  // anchorage_manufacture_locality((void *)tree);
  printf("=============== TRAVERSING (Left then Right) ===============\n");
  num_nodes(tree);
  printf("=============== TRAVERSING (Left then Right) ===============\n");
  num_nodes(tree);

  printf("================= BARRIER ================\n");
  alaska_barrier();
  printf("=============== TRAVERSING (Right then Left) ===============\n");
  num_nodes_rev(tree);
  printf("=============== TRAVERSING (Right then Left) ===============\n");
  num_nodes_rev(tree);
}
