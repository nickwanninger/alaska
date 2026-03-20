#include <alaska/internal/alaska_internal_malloc.h>
#include <sys/mman.h>
#include <string.h>
#include <alaska/util/Logger.hpp>
#include <alaska/util/utils.h>

static constexpr size_t ALIGNMENT = 16;
static constexpr size_t SLAB_SIZE = 2 * 1024 * 1024;  // 2MB
static constexpr size_t LARGE_THRESHOLD = 64 * 1024;   // 64KB

struct AllocHeader {
  size_t size;  // total size including header, always ALIGNMENT-aligned
};

struct FreeChunk {
  size_t size;
  FreeChunk *next;
};

static struct {
  int lock;
  char *slab;
  size_t slab_offset;
  size_t slab_remaining;
  FreeChunk *freelist;
} state;

static inline size_t align_up(size_t n, size_t a) {
  return (n + a - 1) & ~(a - 1);
}

static inline void spin_lock() {
  while (__atomic_test_and_set(&state.lock, __ATOMIC_ACQUIRE)) {
    // spin
  }
}

static inline void spin_unlock() {
  __atomic_clear(&state.lock, __ATOMIC_RELEASE);
}

static void *alloc_large(size_t alloc_size) {
  void *p = mmap(nullptr, alloc_size, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (p == MAP_FAILED) return nullptr;
  auto *hdr = static_cast<AllocHeader *>(p);
  hdr->size = alloc_size;
  return reinterpret_cast<char *>(p) + ALIGNMENT;
}

static void *alloc_small(size_t alloc_size) {
  spin_lock();
  alaska::printf("Allocating small chunk of size %zu\n", alloc_size);
  // alaska_dump_backtrace();

  // Check freelist for a suitable chunk
  FreeChunk **prev = &state.freelist;
  for (FreeChunk *c = state.freelist; c != nullptr; c = c->next) {
    if (c->size >= alloc_size) {
      *prev = c->next;
      spin_unlock();
      auto *hdr = reinterpret_cast<AllocHeader *>(c);
      hdr->size = alloc_size;
      return reinterpret_cast<char *>(hdr) + ALIGNMENT;
    }
    prev = &c->next;
  }

  // Bump-allocate from current slab
  if (state.slab_remaining < alloc_size) {
    void *p = mmap(nullptr, SLAB_SIZE, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) {
      spin_unlock();
      return nullptr;
    }
    state.slab = static_cast<char *>(p);
    state.slab_offset = 0;
    state.slab_remaining = SLAB_SIZE;
  }

  char *ptr = state.slab + state.slab_offset;
  state.slab_offset += alloc_size;
  state.slab_remaining -= alloc_size;
  spin_unlock();

  auto *hdr = reinterpret_cast<AllocHeader *>(ptr);
  hdr->size = alloc_size;
  return ptr + ALIGNMENT;
}

extern "C" void *alaska_internal_malloc(size_t size) {
  if (size == 0) size = 1;
  size_t alloc_size = align_up(size + ALIGNMENT, ALIGNMENT);
  if (alloc_size >= LARGE_THRESHOLD)
    return alloc_large(alloc_size);
  return alloc_small(alloc_size);
}

extern "C" void alaska_internal_free(void *ptr) {
  if (!ptr) return;
  auto *hdr = reinterpret_cast<AllocHeader *>(static_cast<char *>(ptr) - ALIGNMENT);
  size_t alloc_size = hdr->size;

  if (alloc_size >= LARGE_THRESHOLD) {
    munmap(hdr, alloc_size);
    return;
  }

  auto *chunk = reinterpret_cast<FreeChunk *>(hdr);
  chunk->size = alloc_size;
  spin_lock();
  chunk->next = state.freelist;
  state.freelist = chunk;
  spin_unlock();
}

extern "C" void *alaska_internal_calloc(size_t num, size_t size) {
  size_t total = num * size;
  void *p = alaska_internal_malloc(total);
  if (!p) return nullptr;
  // mmap already zeros pages, but freelist reuse may not be zeroed
  memset(p, 0, total);
  return p;
}
