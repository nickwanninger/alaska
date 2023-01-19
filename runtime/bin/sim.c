#include <alaska/trace.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <alaska/rbtree.h>


extern void (*alaska_barrier_hook)(void*, void *, size_t);

struct rb_root root = RB_ROOT;
static uint64_t handle_count = 0;

// an allocation in the trace simulator
typedef struct {
  struct rb_node node;
  uint64_t ptr;
  size_t size;
  void *handle;  // internal trace handle
} alloc_t;



static alloc_t *trace_find(uint64_t va) {
  // walk...
  struct rb_node **n = &(root.rb_node);

  /* Figure out where to put new node */
  while (*n != NULL) {
    alloc_t *r = rb_entry(*n, alloc_t, node);

    off_t start = (off_t)r->ptr;
    off_t end = start + r->size;

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

void *translate(void *p) {
  handle_t h;
  h.ptr = p;
  if (likely(h.flag != 0)) {
    alaska_mapping_t *ent = (alaska_mapping_t *)(uint64_t)h.handle;
    return (void *)((uint64_t)ent->ptr + h.offset);
  }
  return p;
}

void log_action(void *ptr, size_t size, const char *c) {
  static uint64_t access_index = 0;
  fprintf(stderr, "%zu,%zu,%zu,%s\n", access_index++, (uint64_t)ptr, size, c);
}

void trace_alloc(struct alaska_trace_alloc *op) {
  alloc_t *a = calloc(1, sizeof(*a));
  a->ptr = op->ptr;
  a->size = op->size;

  a->handle = halloc(a->size);
  handle_count++;
  log_action(translate(a->handle), a->size, "alloc");

  rb_insert(&root, &a->node, __insert_callback, (void *)a);
}

void trace_realloc(struct alaska_trace_realloc *op) {
  alloc_t *a = trace_find(op->old_ptr);
  if (a == NULL) return;

  // remove the value from the tree
  rb_erase(&a->node, &root);

  // update it's size and location
  log_action(translate(a->handle), a->size, "free");
  a->ptr = op->new_ptr;
  a->size = op->new_size;
  a->handle = hrealloc(a->handle, a->size);
  log_action(translate(a->handle), a->size, "alloc");


  // and add it again
  rb_insert(&root, &a->node, __insert_callback, (void *)a);
}

void trace_free(struct alaska_trace_free *op) {
  alloc_t *a = trace_find(op->ptr);
  if (a == NULL) return;

  log_action(translate(a->handle), a->size, "free");

  // remove the value from the tree
  rb_erase(&a->node, &root);
  handle_count--;
  hfree(a->handle);
  free(a);
}

void trace_lock(struct alaska_trace_lock *op) {
  alloc_t *a = trace_find(op->ptr);
  if (a == NULL) return;
  void *ptr = alaska_lock(a->handle);
  (void)ptr;
  //   uint8_t cls = alaska_get_classification(a->handle);
  //   const char *cls_string = "lock_UNDEFINED";
  //
  //   switch (cls) {
  // #define __CLASS(name, id)       \
//   case id:                      \
//     cls_string = "lock_" #name; \
//     break;
  // #include "../include/classes.inc"
  // #undef __CLASS
  //   }
  // ptr = (void*)((uint64_t)a->handle >> 32);
  log_action(ptr, a->size, "lock");
}

void trace_unlock(struct alaska_trace_unlock *op) {
  alloc_t *a = trace_find(op->ptr);
  if (a == NULL) return;
  alaska_unlock(a->handle);
}


void trace_classify(struct alaska_trace_classify *op) {
  alloc_t *a = trace_find(op->ptr);
  if (a == NULL) return;
  // apply classification
  alaska_classify(a->handle, op->class_id);
}


void trace_defrag_hook(void *oldptr, void *newptr, size_t size) {
  log_action(oldptr, size, "free");
  log_action(newptr, size, "alloc");
}

void trace_barrier(struct alaska_trace_barrier *op) { alaska_barrier(); }

int main(int argc, char **argv) {
  const char *path = "alaska.trace";

  if (argc == 2) {
    path = argv[1];
  }
  int ret;
  size_t len_file;
  struct stat st;
  int fd = 0;
  void *addr = NULL;

  if ((fd = open(path, O_RDONLY)) < 0) {
    perror("Error in file opening");
    return EXIT_FAILURE;
  }

  if ((ret = fstat(fd, &st)) < 0) {
    perror("Error in fstat");
    return EXIT_FAILURE;
  }

  len_file = st.st_size;

  /*len_file having the total length of the file(fd).*/

  if ((addr = mmap(NULL, len_file, PROT_READ, MAP_SHARED, fd, 0)) == MAP_FAILED) {
    perror("Error in mmap");
    return EXIT_FAILURE;
  }

	alaska_barrier_hook = trace_defrag_hook;

  char *cur = addr;
  char *end = cur + len_file;

  long iter = 0;
  while (cur < end) {
    if (iter++ % 100000 == 0) {  // every so often, run a barrier
    	// alaska_barrier();
		}

    // if (iter > 1000000) break;
    uint8_t action = *cur;

    switch (action) {
      case 'A':  // alloc
        trace_alloc((void *)cur);
        cur += sizeof(struct alaska_trace_alloc);
        break;
      case 'R':  // realloc
        trace_realloc((void *)cur);
        cur += sizeof(struct alaska_trace_realloc);
        break;
      case 'F':  // free
        trace_free((void *)cur);
        cur += sizeof(struct alaska_trace_free);
        break;
      case 'L':  // lock
        trace_lock((void *)cur);
        cur += sizeof(struct alaska_trace_lock);
        break;
      case 'U':  // unlock
        trace_unlock((void *)cur);
        cur += sizeof(struct alaska_trace_unlock);
        break;
      case 'C':  // classify
        trace_classify((void *)cur);
        cur += sizeof(struct alaska_trace_classify);
        break;
      case 'B':  // barrier
        trace_barrier((void *)cur);
        cur += sizeof(struct alaska_trace_barrier);
        break;
      default:
        exit(EXIT_FAILURE);
        fprintf(stderr, "unhandled op %02x %zd\n", action, (ssize_t)(cur - (char *)addr));
        for (int i = -16; i < 16; i++) {
          if (i == 0) fprintf(stderr, "\033[31m");
          fprintf(stderr, "%02x ", (uint8_t)cur[i]);
          if (i == 0) fprintf(stderr, "\033[0m");
        }
        fprintf(stderr, "  ");

        for (int i = -16; i < 16; i++) {
          char c = cur[i];
          if (i == 0) fprintf(stderr, "\033[31m");
          fprintf(stderr, "%c", (c < 0x20) || (c > 0x7e) ? '.' : c);
          if (i == 0) fprintf(stderr, "\033[0m");
        }
        fprintf(stderr, "\n");
        // cur++;
        exit(EXIT_FAILURE);
    }
  }

  close(fd);
  return 0;
}
