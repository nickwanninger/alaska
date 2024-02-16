#include <stdio.h>
#include <stdint.h>



// This program is the example program from `Layout Transformations for
// Heap Objects using Static Access Patterns' by Jeon et. al.
struct node {
  int key;
  char *data;
  int miss_count;
  struct node *left;
  struct node *right;
};

int global = 0;


char *search(struct node *h, int k) {
  while (h != NULL) {
    int cur_key = h->key;
    if (cur_key == k) return h->data;

    puts(h->data);
    if (k < cur_key) {
      h = h->left;
    } else if (k > cur_key) {
      h = h->right;
    }
  }
  return NULL;
}

// int count_nodes(struct node *h) {
//   if (h == NULL) return 0;
//   int x = 1;
//   x += count_nodes(h->left);
//   x += count_nodes(h->right);
//
//   return x;
// }

int main() {}
