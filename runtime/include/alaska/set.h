#pragma once

// This header transitively defines all `alaska_alloc*` functions
#include <stdbool.h>
#include <alaska/rbtree.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  struct rb_root root;
} alaska_set_t;

typedef struct {
  struct rb_node node;
  void *value;
} alaska_set_entry_t;

void alaska_set_init(alaska_set_t *set);
void alaska_set_destroy(alaska_set_t *set, void (*free_func)(void *));


// Don't use these functions directly, instead use the non '_' macro versions

// Insert a value into the set
void _alaska_set_insert(alaska_set_t *set, void *value);
// Remove a value from the set. This will not attempt to free value
void _alaska_set_remove(alaska_set_t *set, void *value);
// Return if a value exists in the set
bool _alaska_set_contains(alaska_set_t *set, void *value);


#define alaska_set_insert(set, value) _alaska_set_insert((set), (void*)(unsigned long)(value))
#define alaska_set_remove(set, value) _alaska_set_remove((set), (void*)(unsigned long)(value))
#define alaska_set_contains(set, value) _alaska_set_contains((set), (void*)(unsigned long)(value))



alaska_set_entry_t *alaska_set_first(alaska_set_t *set);
alaska_set_entry_t *alaska_set_next(alaska_set_entry_t *ent);


size_t alaska_set_size_slow(alaska_set_t *set);

/**
 * alaska_set_for_each - iterate in post-order over set.
 *
 * @iter:  the 'type *' iteration variable
 * @cur:   a 'alaska_set_entry_t *'. think of it as `it` in C++
 * @set:   a 'alaska_set_t *' to iterate over
 *
 * Note that this macro can handle iterating, but cannot guarentee correctness
 * if the set is modified during iteration. Doing so is undefined behavior and
 * will likely result in missed entries.
 */
// clang-format off
#define alaska_set_for_each_entry(iter, cur, set) \
	for (cur = alaska_set_first(set);               \
			cur != NULL && ({ iter = cur->value; 1; }); cur = alaska_set_next(cur))


#define alaska_set_for_each(iter, set) \
	alaska_set_entry_t *__cur_##iter;             \
	for (__cur_##iter = alaska_set_first(set);    \
			__cur_##iter != NULL && ({ iter = __cur_##iter->value; 1; }); __cur_##iter = alaska_set_next(__cur_##iter))
// clang-format on

#ifdef __cplusplus
}
#endif
