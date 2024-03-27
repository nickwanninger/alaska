#include <stdlib.h>

struct node {
  struct node *left, *right;
};

struct mynode {
  struct node node;
  int value;
  char buffer[512];
};



#define INIT_NODE(node) (node)->left = (node)->right = NULL

void init_node(struct node *node) {
  node->left = NULL;
  node->right = NULL;
}

struct mynode *make_node(int value) {
  struct mynode *n = calloc(1, sizeof(*n));
  INIT_NODE((struct node *)n);
  n->value = value;
  return n;
};


int main() { return 0; }
