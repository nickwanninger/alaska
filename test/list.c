#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <alaska.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct node {
  struct node *next;
  // char *payload;
} node_t;


unsigned int seed = 12345;


node_t *make_list(int depth) {
  node_t *list = NULL;


  while (depth > 0) {
    size_t size = sizeof(node_t) + depth;
    node_t *n = calloc(1, size);

    off_t o = n - list;
    printf("node: %zu %p %ld\n", size, n, o);



    n->next = list;
    list = n;
    depth--;
  }

  return list;
}

__attribute__((noinline)) int list_count_nodes(volatile node_t *n) {
  int count = 0;

  while (n != NULL) {
    count++;
    n = n->next;
  }
  return count;
}


void free_list(node_t *n) {
  if (n == NULL) return;

  while (n != NULL) {
    node_t *cur = n;
    n = n->next;
    free(cur);
  }
}

bool localize_structure(uint64_t ptr);



void run_tests() {
  for (int trial = 0; trial < 15; trial++) {
    uint64_t start = alaska_timestamp();
    node_t *n = make_list(1 << 16);
    volatile unsigned long c = 0;
    for (int i = 0; i < 20; i++) {
      c += list_count_nodes(n);
    }
    uint64_t end = alaska_timestamp();

    long walk_time = end - start;

    (void)c;
    printf("%d, %fs\n", trial, walk_time / 1e9f);
    free_list(n);
    // return 0;
    // printf("node count: %lu\n", c);
  }
}


int main() {
  // make_list(1 << 21);
  make_list(250);
  return 0;
  long start, end;
  printf("localized,walk_time\n");
  bool localized = false;
  run_tests();

  return EXIT_SUCCESS;
}
