

typedef struct splay_tree_node_s *splay_tree_node;
typedef struct splay_tree_s *splay_tree;
typedef struct splay_tree_key_s *splay_tree_key;

struct splay_tree_key_s {
  long key;
};

#define attribute_hidden __attribute__((visibility("hidden")))

static inline int splay_compare(splay_tree_key x, splay_tree_key y) {
  if (x->key == y->key) {
    return 0;
  } else if (x->key <= y->key) {
    return -1;
  } else {
    return 1;
  }
}

#ifdef splay_tree_prefix
#define splay_tree_name_1(prefix, name) prefix##_##name
#define splay_tree_name(prefix, name) splay_tree_name_1(prefix, name)
#define splay_tree_node_s splay_tree_name(splay_tree_prefix, splay_tree_node_s)
#define splay_tree_s splay_tree_name(splay_tree_prefix, splay_tree_s)
#define splay_tree_key_s splay_tree_name(splay_tree_prefix, splay_tree_key_s)
#define splay_tree_node splay_tree_name(splay_tree_prefix, splay_tree_node)
#define splay_tree splay_tree_name(splay_tree_prefix, splay_tree)
#define splay_tree_key splay_tree_name(splay_tree_prefix, splay_tree_key)
#define splay_compare splay_tree_name(splay_tree_prefix, splay_compare)
#define splay_tree_lookup splay_tree_name(splay_tree_prefix, splay_tree_lookup)
#define splay_tree_insert splay_tree_name(splay_tree_prefix, splay_tree_insert)
#define splay_tree_remove splay_tree_name(splay_tree_prefix, splay_tree_remove)
#define splay_tree_foreach splay_tree_name(splay_tree_prefix, splay_tree_foreach)
#define splay_tree_callback splay_tree_name(splay_tree_prefix, splay_tree_callback)
#endif

#ifndef splay_tree_c
/* Header file definitions and prototypes.  */

/* The nodes in the splay tree.  */
struct splay_tree_node_s {
  struct splay_tree_key_s key;
  /* The left and right children, respectively.  */
  splay_tree_node left;
  splay_tree_node right;
};

/* The splay tree.  */
struct splay_tree_s {
  splay_tree_node root;
};

typedef void (*splay_tree_callback)(splay_tree_key, void *);

extern splay_tree_key splay_tree_lookup(splay_tree, splay_tree_key);
extern void splay_tree_insert(splay_tree, splay_tree_node);
extern void splay_tree_remove(splay_tree, splay_tree_key);
extern void splay_tree_foreach(splay_tree, splay_tree_callback, void *);
#else /* splay_tree_c */
#ifdef splay_tree_prefix
#include "splay-tree.c"
#undef splay_tree_name_1
#undef splay_tree_name
#undef splay_tree_node_s
#undef splay_tree_s
#undef splay_tree_key_s
#undef splay_tree_node
#undef splay_tree
#undef splay_tree_key
#undef splay_compare
#undef splay_tree_lookup
#undef splay_tree_insert
#undef splay_tree_remove
#undef splay_tree_foreach
#undef splay_tree_callback
#undef splay_tree_c
#endif
#endif /* #ifndef splay_tree_c */

#ifdef splay_tree_prefix
#undef splay_tree_prefix
#endif