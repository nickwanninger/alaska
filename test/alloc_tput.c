#include <alaska.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <assert.h>

struct node {
  // uint8_t val;
  struct node *left, *right;
  uint64_t data;
};


// #define SIZE (2UL << 12)
#define SIZE 256

struct node *array[SIZE];

int main() {
  long start, end;
  srand((unsigned)time(NULL));

	array[0] = malloc(4096);

	return 0;

  for (int trial = 0; trial < 12; trial++) {
    start = alaska_timestamp();
    for (uint64_t i = 0; i < SIZE; i++) {
      struct node *node = malloc(sizeof(struct node));
      node->data = i;
      // node->left = node->right = (void *)42;
      array[i] = node;
    }
    end = alaska_timestamp();
    for (uint64_t i = 0; i < SIZE; i++) {
      assert(array[i]->data == i);
      free(array[i]);
    }
    printf("allocation took %fs\n", (end - start) / 1000.0 / 1000.0 / 1000.0);
  }
  return 0;
}
