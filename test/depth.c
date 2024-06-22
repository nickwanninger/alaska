#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
  int data;
  struct Node* left;
  struct Node* right;
} node_t;


typedef struct state {
  int id;
} state_t;

node_t* createNode(int data) {
  node_t* newNode = (node_t*)malloc(sizeof(node_t));
  newNode->data = data;
  newNode->left = NULL;
  newNode->right = NULL;
  return newNode;
}


node_t* createBinaryTree(state_t* st, int depth) {
  if (depth <= 0) return NULL;
  int data = 0;
  node_t* root = createNode(st->id++);
  root->left = createBinaryTree(st, depth - 1);
  root->right = createBinaryTree(st, depth - 1);
  return root;
}



void printTree(node_t* root, int depth) {
  if (root == NULL) return;
  for (int i = 0; i < depth; i++)
    printf("| ");
  printf("%-3d\n", root->data);
  printTree(root->left, depth + 1);
  printTree(root->right, depth + 1);
}


int sumDepth(node_t* root) {
  if (root == NULL) return 0;
  printf("Visit %d\n", root->data);
  return 1 + sumDepth(root->left) + sumDepth(root->right);
}

int main(int argc, char* argv[]) {
  int depth = 5;
  state_t st;
  st.id = 0;

  node_t* root = createBinaryTree(&st, depth);

  // printTree(root, 0);
  sumDepth(root);
  return 0;
}
