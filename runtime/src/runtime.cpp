#include <alaska.h>
#include <alaska/internal.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)

#define MAP_ENTRY_SIZE sizeof(alaska_mapping_t)  // enforced by a static assert in translation_types.h
#define MAP_GRANULARITY 0x200000LU               // minimum size of a "huge page" if we ever get around to that
#define HANDLE_MARKER (1LLU << 63)
#define GET_ENTRY(handle) ((alaska_mapping_t *)((((uint64_t)(handle)) & ~HANDLE_MARKER) >> 32))
#define GET_OFFSET(handle) ((off_t)(handle)&0xFFFFFFFF)

#define ENT_GET_CANONICAL(ent) (((off_t)(ent) - (off_t)(map)) / MAP_ENTRY_SIZE)
#define GET_CANONICAL(handle) ENT_GET_CANONICAL(GET_ENTRY(handle))


// convert a canonical handle id to a pointer.
#define FROM_CANONICAL(chid) (&map[chid])
// How many map cells are there in the map table?
size_t map_size = 0;
// The memory for the alaska translation map. This lives at 0x200000 and is grown in 2mb chunks
static alaska_mapping_t *map = NULL;

// ...
static uint64_t next_usage_timestamp = 0;



// The alaska allocator is a *very* simple free list allocator. We can do
// this because all handle map entries  are the same size. It is the
// allocations they point to that are variable sized. This means we can
// simply store a linked list of free nodes within the alaska map entrys
// themselves. This turns `halloc` calls into a nice O(1) common case,
// and a `mremap()` call in the worst case (if we need more handles)
static alaska_mapping_t *next_handle = NULL;


extern "C" void *halloc(size_t sz) {
  assert(sz < (1LLU << 32));
  uint64_t handle = HANDLE_MARKER | (((uint64_t)next_handle) << 32);


  alaska_mapping_t *ent = next_handle;
  next_handle = (alaska_mapping_t *)ent->ptr;
  if (next_handle == NULL) {
    fprintf(stderr, "alaska: out of space!\n");
    exit(-1);
  }

  memset(ent, 0, MAP_ENTRY_SIZE);
  ent->size = sz;
  // ent->ptr = NULL;  // don't allocate now. Do it later.
  ent->ptr = malloc(sz);  // just use malloc

  return (void *)handle;
}

// Reallocate a handle
extern "C" void *hrealloc(void *handle, size_t sz) {
  auto ent = GET_ENTRY(handle);
  ent->ptr = realloc(ent->ptr, sz);
  ent->size = sz;
  // realloc in alaska will always return the same pointer...
  // Ahh the benefits of using handles!
  //
  // hopefully, nobody is relying on the output of realloc changing :I
  return handle;
}

extern "C" void hfree(void *ptr) {
  auto ent = GET_ENTRY(ptr);
  // assert(ent->locks == 0);
  free(ent->ptr);
  memset(ent, 0, MAP_ENTRY_SIZE);
  ent->size = 0;
  ent->ptr = (void *)next_handle;
  next_handle = (alaska_mapping_t *)ent;
}


enum alaska_fault_reason {
  NOT_PRESENT,
  OUT_OF_BOUNDS,
};


// Alaska's version of a soft page fault. Note the "noinline". This is because this
// is a slow path, and we don't really care about performance, as this function
// may go out to disk, decompress, decrypt, or something else more interesting :)
//
// This, and alaska_barrier are the main locations where the runtime can be customized
extern "C" __declspec(noinline) void alaska_fault(alaska_mapping_t *ent, alaska_fault_reason reason, off_t offset) {
  if (reason == OUT_OF_BOUNDS) {
    fprintf(stderr, "[FATAL] alaska: out of bound access of handle %ld. Attempt to access byte %zu in a %d byte handle!\n",
        ENT_GET_CANONICAL(ent), offset, ent->size);
    exit(-1);
  }


  if (reason == NOT_PRESENT) {
    // lazy allocate for now.
    ent->ptr = malloc(ent->size);
    return;
  }

  fprintf(stderr, "[FATAL] alaska: unknown fault reason, %d, on handle %ld.\n", reason, ENT_GET_CANONICAL(ent));
  exit(-1);
}


template <typename Fn>
static void foreach_handle(Fn fn) {
  for (size_t i = 0; i < map_size; i++) {
    auto *ent = &map[i];
    if (ent->size != 0) {
      if (!fn(ent)) return;
    }
  }
}


#define MAX_MOVE 32

int alaska_ent_usage_compare(const void *_a, const void *_b) {
  auto a = *(alaska_mapping_t **)_a;
  auto b = *(alaska_mapping_t **)_b;

  return a->usage_timestamp - b->usage_timestamp;
}


int alaska_ent_ptr_compare(const void *_a, const void *_b) {
  auto a = *(void **)_a;
  auto b = *(void **)_b;
  return (off_t)a - (off_t)b;
}

// This just means that the application has decided it is willing to let alaska
// potentially stop the world and do what it needs to do. This function doesn't
// do anything yet, but it could :)
extern "C" __declspec(noinline) void alaska_barrier(void) {
  // What size allocation are we looking to relocate?
  uint32_t size_target = 16;
  // How many have we found?
  int found = 0;
  // these are the ones we are interested in
  alaska_mapping_t *relocation_plan[MAX_MOVE];
  void *allocations[MAX_MOVE];

  printf("=== alaska_barrier ===\n");
  // Work through `map` and do something fun
  foreach_handle([&](alaska_mapping_t *ent) {
    if (found >= MAX_MOVE) return false;

    if (ent->locks == 0 && ent->size == size_target) {
      allocations[found] = ent->ptr;
      relocation_plan[found++] = ent;
    }
    return true;
  });

  if (found == 0) return;


  printf("movement plan:\n");
  for (int i = 0; i < found; i++) {
    auto ent = relocation_plan[i];
    printf(" %6lu = { usage:%-4d ptr:%p }\n", ENT_GET_CANONICAL(ent), ent->usage_timestamp, ent->ptr);
  }


  qsort(relocation_plan, found, sizeof(alaska_mapping_t *), alaska_ent_usage_compare);

  printf("relocated:\n");
  for (int i = 0; i < found; i++) {
    auto ent = relocation_plan[i];
    printf(" %6lu = { usage:%-4d ptr:%p }\n", ENT_GET_CANONICAL(ent), ent->usage_timestamp, ent->ptr);
  }


  printf("----------------------\n");
}


// The core function to lock a handle. This is called only once we know
// that @ptr is a handle (indicated by the top bit being set to 1).
extern "C" void *alaska_guarded_lock(void *ptr) {
  // printf("lock %p, %ld\n", ptr, GET_CANONICAL(ptr));
  // A handle is encoded as two 32 bit components. The first (top half)
  // component is simply a pointer to the mapping directly. The second
  // (bottom 32 bits) are an offset into the mapping. This simplifies
  // translation quite a bit, as all handles share the same bit pattern
  // and offset computation is very easy.
  //
  // On AAarch64, this operation can be performed with 3 instructions:
  //   ubfx; ldr; add;
  // (Extract bits (32..63), load `ent->ptr`, and add the offset)
  auto ent = GET_ENTRY(ptr);
  // Extract the lower 32 bits of the pointer for the offset.
  off_t off = GET_OFFSET(ptr);

#ifdef ALASKA_OOB_CHECK
  // Alaska can perform out of bounds checks when each load and store
  // performs a lock. This is also expensive, and should really be outlined
  // by the compiler a-la TEXAS/CARAT.
  //
  // Another option would be to resize pointers automatically, ensuring
  // there is always enough space (within the 2gb space that `off` can
  // measure). This would effectively allow turning allocations into
  // automatically growing arrays.
  if (unlikely(off >= ent->size)) {
    alaska_fault(ent, OUT_OF_BOUNDS, off);
  }
#endif


  // if the pointer is NULL, we need to perform a "handle fault" This
  // is to allow the runtime to fully deallocate unused handles, but it
  // is a relatively expensive check on some architectures...
  // if (unlikely(ent->ptr == NULL)) {
  //   alaska_fault(ent, NOT_PRESENT, off);
  // }

  // Proxy for temporal locality
  ent->usage_timestamp = next_usage_timestamp++;

  // Record the lock occuring so the runtime knows not to relocate the memory.
  // TODO: hoist this into the compiler and avoid doing it if it's not needed.
  // ex: if there are no calls to alaska_barrier between lock/unlocks,
  //     there is no need to lock (we only care about one thread for now)
  ent->locks++;


  // Return the address of the pointer plus the offset we are locking at.
  return (void *)((uint64_t)ent->ptr | off);
}


extern "C" void alaska_guarded_unlock(void *ptr) {
  auto ent = GET_ENTRY(ptr);
  ent->locks--;
}

extern "C" void *alaska_lock(void *ptr) {
  uint64_t h = (uint64_t)ptr;
  if (likely((h & HANDLE_MARKER) != 0)) {
    return alaska_guarded_lock(ptr);
  }
  return ptr;
}

void alaska_unlock(void *ptr) {
  uint64_t h = (uint64_t)ptr;
  if (likely((h & HANDLE_MARKER) != 0)) {
    alaska_guarded_unlock(ptr);
  }
}



#define MAP_HUGE_2MB (21 << MAP_HUGE_SHIFT)

static void __attribute__((constructor)) alaska_init(void) {
  int fd = -1;
  int flags = MAP_PRIVATE | MAP_ANONYMOUS;
  size_t sz = MAP_GRANULARITY * 8;

  // TODO: do this using hugetlbfs :)
  map = (alaska_mapping_t *)mmap((void *)MAP_GRANULARITY, sz, PROT_READ | PROT_WRITE, flags | MAP_FIXED, fd, 0);
  map_size = sz / MAP_ENTRY_SIZE;

  for (size_t i = 0; i < map_size - 1; i++) {
    // when unmapped, the pointer field points to the next free handle
    map[i].ptr = (void *)&map[i + 1];
    map[i].size = 0;  // a size of zero indicates that this field is unmapped
  }
  map[map_size - 1].ptr = NULL;
  // start allocating at the first handle
  next_handle = &map[0];
}

static void __attribute__((destructor)) alaska_deinit(void) { munmap(map, map_size * MAP_ENTRY_SIZE); }

extern "C" void *hmalloc_map_frame(int level, size_t entry_size, int size) {
  void *p = calloc(entry_size, size);
  return p;
}


void *operator new(size_t size) {
  void *ptr = malloc(size);
  return ptr;
}

void operator delete(void *ptr) { free(ptr); }
