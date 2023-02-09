#include <alaska/internal.h>
#include <alaska/rbtree.h>



#ifdef SIM_DEBUG
#define PRINT printf
#else
#define PRINT(...)
#endif

static struct rb_root root = RB_ROOT;
typedef struct {
  struct rb_node node;
  alaska_mapping_t *handle;
} alloc_t;


static alloc_t *trace_find(uint64_t va) {
  // walk...
  struct rb_node **n = &(root.rb_node);

  /* Figure out where to put new node */
  while (*n != NULL) {
    alloc_t *r = rb_entry(*n, alloc_t, node);

    off_t start = (off_t)r->handle->ptr;
    off_t end = start + r->handle->size;

    if (va < start) {
      n = &((*n)->rb_left);
    } else if (va >= end) {
      n = &((*n)->rb_right);
    } else {
      return r;
    }
  }

  return NULL;
}


static inline int __insert_callback(struct rb_node *n, void *arg) {
  alloc_t *allocation = arg;
  alloc_t *other = rb_entry(n, alloc_t, node);
  // do we go left right or here?
  long result = (long)allocation->handle->ptr - (long)other->handle->ptr;

  if (result < 0) {
    return RB_INSERT_GO_LEFT;
  }
  if (result > 0) {
    return RB_INSERT_GO_RIGHT;
  }
  return RB_INSERT_GO_HERE;
}

static void dump(void) {
#ifdef SIM_DEBUG
  struct rb_node *node = rb_first(&root);
	PRINT(" == sim dump ==\n");
  while (node) {
    alloc_t *r = rb_entry(node, alloc_t, node);
    PRINT("  %p -> %p, %zu\n", r->handle->ptr, r->handle, r->handle->size);
    node = rb_next(node);
  }
	PRINT(" ==============\n");
#endif
}


void sim_on_alloc(alaska_mapping_t *m) {
  alloc_t *a = calloc(1, sizeof(*a));
  a->handle = m;
  rb_insert(&root, &a->node, __insert_callback, (void *)a);
  PRINT("alloc %p\n", m->ptr);
  dump();
}

void sim_on_free(alaska_mapping_t *m) {
  alloc_t *a = trace_find((uint64_t)m->ptr);
  PRINT("free %p\n", m->ptr);
  if (a != NULL) {
    // simply remove it from the list
    rb_erase(&a->node, &root);
		free(a);
  }
  dump();
}

void sim_on_realloc(alaska_mapping_t *m, void *new_ptr, size_t new_size) {
  PRINT("realloc %p -> %p\n", m->ptr, new_ptr);
	dump();
  alloc_t *a = trace_find((uint64_t)m->ptr);

  if (a != NULL) {
    // remove the value from the tree
    rb_erase(&a->node, &root);

    // update it's size and location
    a->handle->ptr = new_ptr;
    a->handle->size = new_size;
    // printf("R %p\n", a->handle);

    // and add it again
    rb_insert(&root, &a->node, __insert_callback, (void *)a);
  }
  dump();
}


void *sim_translate(void *ptr) { return ptr; }

alaska_mapping_t *sim_lookup(void *ptr) {
  alloc_t *a = trace_find((uint64_t)ptr);
  if (a != NULL) {
    PRINT("translate %p -> %p\n", ptr, a->handle);
    dump();
    return a->handle;
  }
  return NULL;
}
