#include <stdio.h>
#include <alaska/internal.h>

// The definition of what a handle's bits mean when they are used like a pointer
typedef union {
  struct {
    unsigned long offset : 63;
    unsigned flag : 1;
  };
  void *ptr;
} flagged_pointer_t;

ALASKA_INLINE void *alaska_prefetch_translate(void *ptr) {
  flagged_pointer_t h;
  h.ptr = ptr;
  if (h.flag) {
    printf("Prefetch %p\n", ptr);
  }
  return ptr;
}


ALASKA_INLINE void alaska_prefetch_release(void *ptr) {
  // intentionally nothing
}
