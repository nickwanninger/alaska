#include <stdlib.h>

struct node {
  int c;
  struct node *left;
  struct node *right;
};

struct node *find(struct node *cur, int n) {
  while (1) {
    if (cur == NULL) return NULL;
    if (cur->c == n) return cur;

    if (cur->c < n) {
      cur = cur->left;
    } else {
      cur = cur->right;
    }
  }
}

int main() {}