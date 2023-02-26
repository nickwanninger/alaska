/*
 * This file is part of the Alaska Handle-Based Memory Management System
 *
 * Copyright (c) 2023, Nick Wanninger <ncw@u.northwestern.edu>
 * Copyright (c) 2023, The Constellation Project
 * All rights reserved.
 *
 * This is free software.  You are permitted to use, redistribute,
 * and modify it as specified in the file "LICENSE".
 */

#include <alaska/trace.h>
#include <alaska/rbtree.h>
#include <alaska/internal.h>
#include <alaska.h>
#include <string.h>
#include <unistd.h>

struct rb_root root = RB_ROOT;
static int tracing_pid = -1;
typedef struct {
  struct rb_node node;
  uint64_t ptr;
  size_t size;
} alloc_t;

static alloc_t *trace_find(uint64_t va) {
  // walk...
  struct rb_node **n = &(root.rb_node);

  /* Figure out where to put new node */
  while (*n != NULL) {
    alloc_t *r = rb_entry(*n, alloc_t, node);

    uint64_t start = (off_t)r->ptr;
    uint64_t end = start + r->size;

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
  long result = (long)allocation->ptr - (long)other->ptr;

  if (result < 0) {
    return RB_INSERT_GO_LEFT;
  }
  if (result > 0) {
    return RB_INSERT_GO_RIGHT;
  }
  return RB_INSERT_GO_HERE;
}



static void close_trace_file(void);
static FILE *get_trace_file(void) {
  static FILE *stream = NULL;
  if (stream == NULL) {
    tracing_pid = gettid();
    atexit(close_trace_file);
    stream = fopen("alaska.trace", "w");
  }
  return stream;
}

static void close_trace_file(void) { fclose(get_trace_file()); }

static void trace_write(void *data, size_t sz) {
  char c = *(char *)data;
  switch (c) {
    case 'A': // alloc
    case 'R': // realloc
    case 'F': // free
    case 'L': // lock
    case 'U': // unlock
    case 'C': // classify
    case 'B': // barrier
      break;

    default:
      fprintf(stderr, "invalid trace type %c\n", c);
      exit(EXIT_FAILURE);
      break;
  }

  FILE *stream = get_trace_file();
  if (gettid() != tracing_pid) return;
  fwrite(data, sz, 1, stream);
}

#define TRACE(struct) trace_write(&struct, sizeof(struct));

void *halloc_trace(size_t sz) {
  void *p = malloc(sz);
  struct alaska_trace_alloc t;
  t.type = 'A';
  t.ptr = (uint64_t)p;
  t.size = (uint64_t)sz;
  TRACE(t);


  alloc_t *a = calloc(1, sizeof(*a));
  a->ptr = (uint64_t)p;
  a->size = sz;
  rb_insert(&root, &a->node, __insert_callback, (void *)a);


  return p;
}


void *hcalloc_trace(size_t nmemb, size_t size) {
  void *p = halloc_trace(nmemb * size);
  memset(p, 0, nmemb * size);
  return p;
}


void *hrealloc_trace(void *ptr, size_t new_size) {
  if (ptr == NULL) return halloc_trace(new_size);

  void *new_ptr = realloc(ptr, new_size);

  alloc_t *a = trace_find((uint64_t)ptr);
  if (a != NULL) {
    // remove the value from the tree
    rb_erase(&a->node, &root);

    // update it's size and location
    a->ptr = (uint64_t)new_ptr;
    a->size = new_size;
    // printf("R %p\n", a->handle);

    // and add it again
    rb_insert(&root, &a->node, __insert_callback, (void *)a);

    struct alaska_trace_realloc t;
    t.type = 'R';
    t.old_ptr = (uint64_t)ptr;
    t.new_ptr = (uint64_t)new_ptr;
    t.new_size = (uint64_t)new_size;
    TRACE(t);
  }

  return new_ptr;
}


void hfree_trace(void *ptr) {
  alloc_t *a = trace_find((uint64_t)ptr);
  if (a != NULL) {
    struct alaska_trace_free t;
    t.type = 'F';
    t.ptr = (uint64_t)ptr;
    TRACE(t);
    rb_erase(&a->node, &root);
    free(a);
  }

  free(ptr);
}



void *alaska_lock_trace(void *restrict ptr) {
  alloc_t *a = trace_find((uint64_t)ptr);
  if (a != NULL) {
    struct alaska_trace_lock t;
    t.type = 'L';
    t.ptr = (uint64_t)ptr;
    TRACE(t);
  }

  return ptr;
}

void alaska_unlock_trace(void *restrict ptr) {
  alloc_t *a = trace_find((uint64_t)ptr);
  if (a != NULL) {
    struct alaska_trace_unlock t;
    t.type = 'U';
    t.ptr = (uint64_t)ptr;
    TRACE(t);
  }
}


void alaska_classify_trace(void *restrict ptr, uint8_t c) {
  alloc_t *a = trace_find((uint64_t)ptr);
  if (a != NULL) {
    struct alaska_trace_classify t;
    t.type = 'C';
    t.ptr = (uint64_t)ptr;
    t.class_id = c;
    TRACE(t);
  }
}

void alaska_barrier_trace(void) {
	// just write out that we did a barrier
  struct alaska_trace_barrier t;
  t.type = 'B';
  TRACE(t);
}
