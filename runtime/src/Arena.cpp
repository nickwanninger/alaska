#include <stdio.h>
#include <alaska/Arena.hpp>
#include "alaska/translation_types.h"
#include <math.h>


// automatically generated in the driller file, (runtime/src/translate.c)
extern alaska_map_driller_t alaska_drillers[32];

alaska::Arena::Arena(int id) : m_id(id) {
  printf("Arena %d created\n", m_id);
  for (uint64_t i = 0; i < 32; i++) {
    size_t size = 2LLU << (i + 5);
    m_tables[i].init(size, alaska_drillers[i]);
  }
}
alaska::Arena::~Arena(void) {}


alaska_handle_t alaska::Arena::allocate(size_t size) {
  // TODO: this rounding code can 100% be faster and less complicated.  All that
  // happens here is we round up to the nearest power of 2, clap it at a min of
  // 64, then compute the "bin" value, which is the index into m_tables, and the
  // value in the high bits of the handle
  size_t bin_size = size == 1 ? 1 : 1 << (64 - __builtin_clzl(size - 1));
  if (bin_size < 64) bin_size = 64;  // clap at a min of 64 (TODO: 128 is better util)
  int bin = log2(bin_size) - 6;      // real size is `2^(bin + 6)`
  if (bin > 32) {
    fprintf(stderr, "alaska: cannot allocate handle of size %zu. Too big.\n", size);
    abort();
  }

  auto &table = m_tables[bin];
  alaska_map_entry_t *entry = NULL;

  // TODO: SLOW AND BAD, FIX
  // find a place to put the value.
  alaska_handle_t handle_id;
  for (handle_id = bin_size;; handle_id += bin_size) {
    // printf("trying %lx\n", handle_id);
    entry = table.translate(handle_id, AllocateIntermediate::Yes);
    if (entry->ptr == NULL) {
      break;
    }
  }
  alaska_handle_t handle = ALASKA_INDICATOR | ALASKA_AID(m_id) | ALASKA_BIN(bin) | handle_id;
  // printf("Allocate sz=%zu, bin=%d, handle=%lx\n", size, bin, handle);

  entry->pindepth = 0;
  entry->ptr = malloc(size);
  return handle;
}

void alaska::Arena::free(alaska_handle_t handle) {
  int bin = ALASKA_GET_BIN(handle);
  // printf("Free %lx, bin=%d\n", handle, bin);
  auto *entry = m_tables[bin].translate(handle, AllocateIntermediate::No);
  if (entry == NULL) {
    fprintf(stderr, "alaska: invalid call to free with handle, %lx.\n", handle);
    abort();
  }
  if (entry->pindepth != 0) {
    fprintf(stderr, "alaska: warning, handle %lx was not fully unpinned before freeing. UAF likely!\n", handle);
  }

  ::free(entry->ptr);
  entry->ptr = 0;
}

void *alaska::Arena::pin(alaska_handle_t handle) {
  int bin = ALASKA_GET_BIN(handle);
  auto *entry = m_tables[bin].translate(handle, AllocateIntermediate::No);
  if (entry == NULL) {
    fprintf(stderr, "alaska: invalid call to `pin` with handle, %lx.\n", handle);
    abort();
  }
  // entry->pindepth++;

  off_t offset = (off_t)handle & (off_t)((2 << (bin + 5)) - 1);

  void *pin = (void *)((off_t)entry->ptr + offset);
  // printf("%p, off=%p, ptr=%p, pin=%p\n", handle, offset, entry->ptr, pin);
  return pin;
}

void alaska::Arena::unpin(alaska_handle_t handle) {
  int bin = ALASKA_GET_BIN(handle);
  auto *entry = m_tables[bin].translate(handle, AllocateIntermediate::No);
  if (entry == NULL) {
    fprintf(stderr, "alaska: invalid call to `pin` with handle, %lx.\n", handle);
    abort();
  }

  // entry->pindepth--;
}

///////////////////////////////////////////////////////////////////////////////

alaska::HandleTable::~HandleTable(void) {
  if (m_lvl0 != NULL) free(m_lvl0);
}

void alaska::HandleTable::init(size_t size, alaska_map_driller_t driller) {
  m_size = size;
  m_driller = driller;
  m_lvl0 = (uint64_t *)alaska_alloc_map_frame(m_size, sizeof(uint64_t *), 512);
}

alaska_map_entry_t *alaska::HandleTable::translate(alaska_handle_t handle, AllocateIntermediate allocate_intermediate) {
  return m_driller(handle, m_lvl0, allocate_intermediate == AllocateIntermediate::Yes);
}