#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <alaska.h>
#include <stdbool.h>

typedef struct node {
  struct node *left, *right;
  int value;
  // char *payload;
} node_t;


unsigned int seed = 12345;

node_t *get_right(node_t *n) {
  node_t **r = &n->right;
  // printf("%zx\n", (uintptr_t)r);
  return *r;
}


char *generate_fake_string(int max_len) {
  int len = 1 + (rand_r(&seed) % max_len);
  char *s = malloc(len + 1);
  for (int i = 0; i < len; i++) {
    s[i] = 'a' + (rand_r(&seed) % 26);
  }
  s[len] = 0;
  return s;
}


node_t *make_tree(int depth) {
  if (depth == 0) return NULL;

  node_t *n = calloc(1, sizeof(*n));

  // n->payload = generate_fake_string(512);

  n->left = make_tree(depth - 1);
  n->right = make_tree(depth - 1);
  n->value = rand_r(&seed);

  return n;
}


void free_tree(node_t *n) {
  if (n == NULL) return;

  free_tree(n->left);
  free_tree(n->right);
  // free(n->payload);
  free(n);
}


int tree_count_nodes(volatile node_t *n) {
  if (n == NULL) return 0;
  return 1 + tree_count_nodes(n->left) + tree_count_nodes(n->right);
}

static void collect_nodes(node_t *n, node_t **nodes, size_t *idx) {
  if (n == NULL) return;
  nodes[(*idx)++] = n;
  collect_nodes(n->left, nodes, idx);
  collect_nodes(n->right, nodes, idx);
}

static void shuffle_nodes(node_t **nodes, size_t count) {
  if (count < 2) return;
  for (size_t i = count - 1; i > 0; i--) {
    size_t j = rand_r(&seed) % (i + 1);
    node_t *tmp = nodes[i];
    nodes[i] = nodes[j];
    nodes[j] = tmp;
  }
}

node_t *jumble_tree(node_t *root) {
  if (root == NULL) return NULL;

  size_t count = (size_t)tree_count_nodes((volatile node_t *)root);
  if (count < 2) return root;

  node_t **nodes = malloc(count * sizeof(*nodes));
  if (nodes == NULL) return root;

  size_t idx = 0;
  collect_nodes(root, nodes, &idx);
  shuffle_nodes(nodes, count);

  for (size_t i = 0; i < count; i++) {
    nodes[i]->left = NULL;
    nodes[i]->right = NULL;
  }

  size_t *available = malloc(count * sizeof(*available));
  if (available == NULL) {
    free(nodes);
    return root;
  }

  size_t available_count = 0;
  node_t *new_root = nodes[0];
  available[available_count++] = 0;

  for (size_t i = 1; i < count; i++) {
    node_t *child = nodes[i];

    size_t parent_slot = rand_r(&seed) % available_count;
    size_t parent_index = available[parent_slot];
    node_t *parent = nodes[parent_index];

    if (parent->left == NULL && parent->right == NULL) {
      if (rand_r(&seed) & 1) {
        parent->left = child;
      } else {
        parent->right = child;
      }
    } else if (parent->left == NULL) {
      parent->left = child;
    } else {
      parent->right = child;
    }

    if (parent->left != NULL && parent->right != NULL) {
      available[parent_slot] = available[--available_count];
    }

    available[available_count++] = i;
  }

  free(available);
  free(nodes);
  return new_root;
}




void run_tests(node_t *n) {


  for (int trial = 0; trial < 15; trial++) {
    volatile unsigned long c = 0;
    uint64_t start = alaska_timestamp();
    for (int i = 0; i < 200; i++) {
      c += tree_count_nodes(n);
    }
    uint64_t end = alaska_timestamp();

    long walk_time = end - start;

    (void)c;
    printf("%d, %fs\n", trial, walk_time / 1e9f);
    // return 0;
    // printf("Node count: %lu\n", c);
  }
}


int main() {
  long start, end;
  node_t *n = jumble_tree(make_tree(18));
  // node_t *n = make_tree(21);
  printf("localized,walk_time\n");
  bool localized = false;
  // return 0;

  // run_tests(n);
  run_tests(n);
  free_tree(n);

  return EXIT_SUCCESS;
}
