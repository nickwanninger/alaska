// Shared by yukon and yukon_stub. No RISC-V code here, so host tests can use it.
#pragma once

#include <errno.h>
#include <stddef.h>
#include <unistd.h>

// The aligned allocation entry points, with glibc's argument rules. `alloc` takes
// (align, size) and needs a power-of-two alignment.
typedef void *(*aligned_alloc_fn)(size_t align, size_t size);

static inline bool is_power_of_two(size_t x) { return x != 0 && (x & (x - 1)) == 0; }

static inline int shared_posix_memalign(
    aligned_alloc_fn alloc, void **memptr, size_t align, size_t size) {
  if (align % sizeof(void *) != 0 || !is_power_of_two(align)) return EINVAL;
  void *p = alloc(align, size);
  if (p == nullptr && size != 0) return ENOMEM;
  *memptr = p;
  return 0;
}

static inline void *shared_aligned_alloc(aligned_alloc_fn alloc, size_t align, size_t size) {
  if (!is_power_of_two(align)) {
    errno = EINVAL;
    return nullptr;
  }
  void *p = alloc(align, size);
  if (p == nullptr && size != 0) errno = ENOMEM;
  return p;
}

// memalign rounds an alignment that is not a power of two up to one.
static inline void *shared_memalign(aligned_alloc_fn alloc, size_t align, size_t size) {
  if (align > ((size_t)-1 >> 1) + 1) {
    errno = EINVAL;
    return nullptr;
  }
  size_t a = 1;
  while (a < align) a <<= 1;
  return shared_aligned_alloc(alloc, a, size);
}

static inline void *shared_valloc(aligned_alloc_fn alloc, size_t size) {
  return shared_memalign(alloc, getpagesize(), size);
}
