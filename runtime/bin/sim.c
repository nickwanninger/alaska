#include <alaska/trace.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <alaska/rbtree.h>


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




void trace_alloc(struct alaska_trace_alloc *op) {
  alloc_t *a = calloc(1, sizeof(*a));
  a->ptr = op->ptr;
  a->size = op->size;

  a->handle = halloc(a->size);
  handle_count++;
  // printf("A %p\n", a->handle);

  rb_insert(&root, &a->node, __insert_callback, (void *)a);
}

void trace_realloc(struct alaska_trace_realloc *op) {
  alloc_t *a = trace_find(op->old_ptr);
  if (a == NULL) return;

  // remove the value from the tree
  rb_erase(&a->node, &root);

  // update it's size and location
  a->ptr = op->new_ptr;
  a->size = op->new_size;
  a->handle = hrealloc(a->handle, a->size);
  // printf("R %p\n", a->handle);

  // and add it again
  rb_insert(&root, &a->node, __insert_callback, (void *)a);
}

void trace_free(struct alaska_trace_free *op) {
  alloc_t *a = trace_find(op->ptr);
  if (a == NULL) return;
  // printf("F %p\n", a->handle);

  // remove the value from the tree
  rb_erase(&a->node, &root);
  handle_count--;
  hfree(a->handle);
  free(a);
}

static uint64_t access_index = 0;
void trace_lock(struct alaska_trace_lock *op) {
  alloc_t *a = trace_find(op->ptr);
  if (a == NULL) return;
	fprintf(stderr, "%zu,%zu\n", access_index++, (uint64_t)op->ptr);
  alaska_lock(a->handle);
}

void trace_unlock(struct alaska_trace_unlock *op) {
  alloc_t *a = trace_find(op->ptr);
  if (a == NULL) return;
  alaska_unlock(a->handle);
}

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


  char *cur = addr;
  char *end = cur + len_file;

  long iter = 0;
  while (cur < end) {
    float progress = 1.0f - (float)(end - cur) / (float)len_file;
    if (iter++ > 10000000) {
      printf("%6.2f%% %zu\n", progress * 100.0f, handle_count);
      iter = 0;
    }
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
      default:
        fprintf(stderr, "unhandled op %02x %zd\n", action, (ssize_t)(cur - (char*)addr));
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
