#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <alaska.h>


// just include the c file so its easier to build :)
#include "splay-tree.c"
#include "rbtree.c"


long *keys;
long *lookups;

#define mallocz(n) memset(malloc(n), 0, n)


void run(void *state, void (*insert)(void *, long), long *(*lookup)(void *, long)) {
  int has_looked_up = 0;
  char buf[512];
  // read each line from stdin
  while (!feof(stdin)) {
    long value = 0;
    buf[0] = '\0';  // be "safe"
    fgets(buf, 512, stdin);

    if (sscanf(buf, "insert %ld\n", &value) == 1) {
      printf("insert %ld\n", value);
      insert(state, value);
      continue;
    }


    if (sscanf(buf, "lookup %ld\n", &value) == 1) {
      if (!has_looked_up) {
        // anchorage_manufacture_locality(state);
        alaska_barrier();
      }
      has_looked_up = 1;
      printf("lookup %ld\n", value);
      long *found = lookup(state, value);
      printf("found = %p\n", found);
      continue;
    }
  }
}



void test_splay_insert(void *state, long value) {
  splay_tree s = state;
  splay_tree_node n = (splay_tree_node)mallocz(sizeof(*n));
  n->key.key = value;
  splay_tree_insert(s, n);
}

long *test_splay_lookup(void *state, long value) {
  splay_tree s = state;
  struct splay_tree_key_s lookup_key;

  lookup_key.key = value;
  volatile splay_tree_key result;

  result = splay_tree_lookup(s, &lookup_key);

  if (result) {
    return &result->key;
  } else {
    return NULL;
  }
}


typedef struct {
  struct rb_node node;
  long value;
} rb_node_t;

static inline int __rb_insert_callback(struct rb_node *n, void *arg) {
  rb_node_t *a = arg;
  rb_node_t *other = rb_entry(n, rb_node_t, node);
  // do we go left right or here?
  long result = (long)a->value - (long)other->value;

  if (result < 0) {
    return RB_INSERT_GO_LEFT;
  }
  if (result > 0) {
    return RB_INSERT_GO_RIGHT;
  }
  return RB_INSERT_GO_HERE;
}


void test_rb_insert(void *state, long value) {
  struct rb_root *root = state;

  rb_node_t *a = mallocz(sizeof(*a));
  a->value = value;

  rb_insert(root, &a->node, __rb_insert_callback, (void *)a);
}

long *test_rb_lookup(void *state, long value) {
  struct rb_root *root = state;

  // walk...
  struct rb_node **n = &(root->rb_node);

  /* Figure out where to put new node */
  while (*n != NULL) {
    rb_node_t *r = rb_entry(*n, rb_node_t, node);

    long result = (long)value - (long)r->value;

    if (result < 0) {
      n = &((*n)->rb_left);
    } else if (result > 0) {
      n = &((*n)->rb_right);
    } else {
      return &r->value;
    }
  }
  return NULL;
}




int main(int argc, char **argv) {
  const char *which = "splay";

  if (argc > 1) {
    which = argv[1];
  }

  if (!strcmp(which, "splay")) {
    splay_tree s = (splay_tree)mallocz(sizeof(*s));
    run(s, test_splay_insert, test_splay_lookup);
    return EXIT_SUCCESS;
  }

  if (!strcmp(which, "rb")) {
    struct rb_root *s = (struct rb_root *)mallocz(sizeof(*s));
    run(s, test_rb_insert, test_rb_lookup);
    return EXIT_SUCCESS;
  }
}
