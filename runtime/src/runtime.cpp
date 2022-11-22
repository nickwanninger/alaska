#include <alaska.h>
#include <alaska/translation_types.h>
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

#define MAP_ENTRY_SIZE 16           // enforced by a static assert in translation_types.h
#define MAP_GRANULARITY 0x200000LU  // minimum size of a "huge page" if we ever get around to that
#define HANDLE_MARKER (1LLU << 63)
#define GET_ENTRY(handle) ((alaska_map_entry_t *)((((uint64_t)(handle)) & ~HANDLE_MARKER) >> 32))
#define GET_OFFSET(handle) ((off_t)(handle)&0xFFFFFFFF)

#define ENT_GET_CANONICAL(ent) (((off_t)(ent) - (off_t)(map)) / MAP_ENTRY_SIZE)
#define GET_CANONICAL(handle) ENT_GET_CANONICAL(GET_ENTRY(handle))


// convert a canonical handle id to a pointer.
#define FROM_CANONICAL(chid) (&map[chid])
// How many map cells are there in the map table?
size_t map_size = 0;
// The memory for the alaska translation map. This lives at 0x200000 and is grown in 2mb chunks
static alaska_map_entry_t *map = NULL;

// The alaska allocator is a *very* simple free list allocator. We can do
// this because all handles, in handle space, are the same size. It is the
// allocations they point to that are variable sized. This means we can
// simply store a linked list of free nodes within the alaska map entrys
// themselves. This turns `alaska_alloc` calls into a anice O(1) common case,
// and a `mremap()` call in the worst case (if we need more handles)
static alaska_map_entry_t *next_handle = NULL;



extern "C" void *alaska_alloc(size_t sz) {
  assert(sz < (1LLU << 32));
  uint64_t handle = HANDLE_MARKER | (((uint64_t)next_handle) << 32);


  alaska_map_entry_t *ent = next_handle;
  next_handle = (alaska_map_entry_t *)ent->ptr;
  if (next_handle == NULL) {
    fprintf(stderr, "alaska: out of space!\n");
    exit(-1);
  }

  memset(ent, 0, MAP_ENTRY_SIZE);
  ent->size = sz;
  /// ent->ptr = NULL;  // don't allocate now. Do it later.
  ent->ptr = malloc(sz);  // just use malloc

  return (void *)handle;
}

// Reallocate a handle
extern "C" void *alaska_realloc(void *handle, size_t sz) {
  auto ent = GET_ENTRY(handle);
  ent->ptr = realloc(ent->ptr, sz);
  ent->size = sz;
  // realloc in alaska will always return the same pointer...
  // Ahh the benefits of using handles!
  //
  // hopefully, nobody is relying on the output of realloc changing :I
  return handle;
}

extern "C" void alaska_free(void *ptr) {
  auto ent = GET_ENTRY(ptr);
  // assert(ent->locks == 0);
  free(ent->ptr);
  memset(ent, 0, MAP_ENTRY_SIZE);
  ent->size = 0;
  ent->ptr = (void *)next_handle;
  next_handle = (alaska_map_entry_t *)ent;
}


// Alaska's version of a soft page fault. Note the "noinline". This is because this
// is a slow path, and we don't really care about performance, as this function
// may go out to disk, decompress, decrypt, or something else more interesting :)
//
// This, and alaska_barrier are the main locations where the runtime can be customized
extern "C" __declspec(noinline) void alaska_fault(alaska_map_entry_t *ent) {
  // lazy allocate for now.
  ent->ptr = malloc(ent->size);
}

// This just means that the application has decided it is willing to let alaska
// potentially stop the world and do what it needs to do. This function doesn't
// do anything yet, but it could :)
extern "C" __declspec(noinline) void alaska_barrier(void) {
  // Work through `map` and do something fun
}

extern "C" __declspec(noinline) void alaska_fault_oob(alaska_map_entry_t *ent, off_t offset) {
  fprintf(stderr, "[FATAL] alaska: out of bound access of handle %ld. Attempt to access byte %zu in a %d byte handle!\n",
      ENT_GET_CANONICAL(ent), offset, ent->size);
  exit(-1);
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
  //   ubfx, ldr, add
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
    alaska_fault_oob(ent, off);
  }
#endif

  // if the pointer is NULL, we need to perform a "handle fault" This
  // is to allow the runtime to fully deallocate unused handles, but it
  // is a relatively expensive check on some architectures...
  // if (unlikely(ent->ptr == NULL)) {
  //   alaska_fault(ent);
  // }



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
  map = (alaska_map_entry_t *)mmap((void *)MAP_GRANULARITY, sz, PROT_READ | PROT_WRITE, flags | MAP_FIXED, fd, 0);
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

extern "C" void *alaska_alloc_map_frame(int level, size_t entry_size, int size) {
  void *p = calloc(entry_size, size);
  return p;
}


void *operator new(size_t size) {
  void *ptr = malloc(size);
  return ptr;
}

void operator delete(void *ptr) { free(ptr); }
