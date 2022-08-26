#include <stdio.h>
#include <alaska/set.h>

struct set_entry {
  struct rb_node node;
  void *value;
};

void alaska_set_init(alaska_set_t *set) {
  set->root = RB_ROOT;
  //
}

void alaska_set_destroy(alaska_set_t *set, void (*free_func)(void *)) {
  struct set_entry *n, *node;

  rbtree_postorder_for_each_entry_safe(node, n, &set->root, node) {
    if (free_func != NULL)
      free_func(node->value);
    free(node);
  }
}

size_t alaska_set_size_slow(alaska_set_t *set) {
  size_t count = 0;
  void *i;
  alaska_set_for_each(i, set) {
    count++;
  }
  return count;
}

static alaska_set_entry_t *get_entry(alaska_set_t *set, void *value) {

  off_t va = (off_t)value;

  // walk...
  struct rb_node **n = &(set->root.rb_node);
  struct rb_node *parent = NULL;

  int steps = 0;

  /* Figure out where to put new node */
  while (*n != NULL) {
    alaska_set_entry_t *r = rb_entry(*n, alaska_set_entry_t, node);

    off_t start = (off_t)r->value;
    parent = *n;

    steps++;

    if (va < start) {
      n = &((*n)->rb_left);
    } else if (va > start) {
      n = &((*n)->rb_right);
    } else {
      return r;
    }
  }

  return NULL;
}

static inline int __insert_callback(struct rb_node *n, void *arg) {
  alaska_set_entry_t *ent = arg;
  alaska_set_entry_t *other = rb_entry(n, alaska_set_entry_t, node);
  // do we go left right or here?
  long result = (long)ent->value - (long)other->value;

  if (result < 0) {
    return RB_INSERT_GO_LEFT;
  }
  if (result > 0) {
    return RB_INSERT_GO_RIGHT;
  }
  return RB_INSERT_GO_HERE;
}

void _alaska_set_insert(alaska_set_t *set, void *value) {
  // printf("insert %p\n", value);
  alaska_set_entry_t *ent = calloc(1, sizeof(*ent));
  ent->value = value;
  rb_insert(&set->root, &ent->node, __insert_callback, ent);
}

void _alaska_set_remove(alaska_set_t *set, void *value) {
  alaska_set_entry_t *ent = get_entry(set, value);
  if (ent == NULL)
    return;
  rb_erase(&ent->node, &set->root);
	free(ent);
}

bool _alaska_set_contains(alaska_set_t *set, void *value) {
  return get_entry(set, value) != NULL;
}

alaska_set_entry_t *alaska_set_first(alaska_set_t *set) {
  return rb_entry_safe(rb_first(&set->root), alaska_set_entry_t, node);
}
alaska_set_entry_t *alaska_set_next(alaska_set_entry_t *ent) {
  if (ent == NULL)
    return NULL;

  return rb_entry_safe(rb_next(&ent->node), alaska_set_entry_t, node);
}
