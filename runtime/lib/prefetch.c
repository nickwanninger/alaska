#include <stdio.h>
#include <alaska/internal.h>


ALASKA_INLINE void *alaska_prefetch_translate(void *ptr) {
  // TODO:
  __builtin_prefetch(ptr);
  return ptr;
}


ALASKA_INLINE void alaska_prefetch_release(void *ptr) {
  // intentionally nothing
}
