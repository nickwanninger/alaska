#include <stdio.h>
#include <alaska/alaska.hpp>


extern "C" ALASKA_INLINE void *alaska_prefetch_translate(void *ptr) {
  // TODO:
  __builtin_prefetch(ptr);
  return ptr;
}


extern "C" ALASKA_INLINE void alaska_prefetch_release(void *ptr) {
  // intentionally nothing
}
