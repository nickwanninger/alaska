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
  // find a place to put the value.
  alaska_handle_t handle_id;
  auto *entry = table.allocate_handle(handle_id);
  alaska_handle_t handle = ALASKA_INDICATOR | ALASKA_AID(m_id) | ALASKA_BIN(bin) | handle_id;

  entry->pindepth = 0;
  entry->ptr = malloc(size);

  if (bin_size == 64) {
    handle = ALASKA_INDICATOR | ALASKA_AID(m_id) | ALASKA_BIN(bin) | ((unsigned long)entry << 6);
    // printf("Handle %p, entry %p\n", handle, entry);
  }

  return handle;
}

void alaska::Arena::free(alaska_handle_t handle) {
  return;
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

  alaska_map_entry_t *entry;

  if (bin == 0) {
    // printf("a: %p\n", handle);
    alaska_handle_t th = handle << 9;
    // printf("b: %p\n", th);
    th >>= (9 + 6);
    // printf("c: %p <- entry\n", th);
    // printf("r: %p\n", entry = (alaska_map_entry_t *)((handle & ~(0b111111111LU << 55)) >> 6));
    entry = (alaska_map_entry_t *)th;
  } else {
    entry = m_tables[bin].translate(handle, AllocateIntermediate::No);
  }

  if (entry == NULL) {
    fprintf(stderr, "alaska: invalid call to `pin` with handle, %lx.\n", handle);
    abort();
  }

  entry->pindepth++;

  off_t offset = (off_t)handle & (off_t)((2 << (bin + 5)) - 1);

  void *pin = (void *)((off_t)entry->ptr + offset);
  // printf("%p, off=%p, ptr=%p, pin=%p\n", handle, offset, entry->ptr, pin);
  return pin;
}

void alaska::Arena::unpin(alaska_handle_t handle) {
  return;
  int bin = ALASKA_GET_BIN(handle);
  auto *entry = m_tables[bin].translate(handle, AllocateIntermediate::No);
  if (entry == NULL) {
    fprintf(stderr, "alaska: invalid call to `pin` with handle, %lx.\n", handle);
    abort();
  }

  entry->pindepth--;
}

///////////////////////////////////////////////////////////////////////////////

alaska::HandleTable::~HandleTable(void) {
  if (m_lvl0 != NULL) free(m_lvl0);
}
#define TLB_KEYSIZE 9

void alaska::HandleTable::init(size_t size, alaska_map_driller_t driller) {
  m_next_handle = m_size = size;
  m_driller = driller;
  m_lvl0 = (uint64_t *)alaska_alloc_map_frame(m_size, sizeof(uint64_t *), 512);

#ifdef ALASKA_ENABLE_STLB
  m_tlb = (TLBEntry *)calloc(sizeof(TLBEntry), 1 << TLB_KEYSIZE);

  for (int i = 0; i < TLB_KEYSIZE; i++) {
    m_tlb->key = -1;
    m_tlb->entry = NULL;
  }
#endif
}

extern long alaska_tlb_hits;
extern long alaska_tlb_misses;

#define unlikely(x) __builtin_expect((x), 0)
alaska_map_entry_t *alaska::HandleTable::translate(alaska_handle_t handle, AllocateIntermediate allocate_intermediate) {
#ifndef ALASKA_ENABLE_STLB
  return m_driller(handle, m_lvl0, allocate_intermediate == AllocateIntermediate::Yes);
#else
  int bin = ALASKA_GET_BIN(handle);
  uint64_t key = handle >> (bin + 5);

  int ind = key & ((1 << TLB_KEYSIZE) - 1);

  alaska_map_entry_t *mapping = NULL;

  auto &tlb_entry = m_tlb[ind];
  if (tlb_entry.key == key) {
    alaska_tlb_hits++;
    mapping = tlb_entry.entry;
  } else {
    mapping = m_driller(handle, m_lvl0, allocate_intermediate == AllocateIntermediate::Yes);

    alaska_tlb_misses++;
    if (mapping) {
      tlb_entry.key = key;
      tlb_entry.entry = mapping;
    }
  }

  return mapping;
#endif
}



alaska_map_entry_t *alaska::HandleTable::allocate_handle(alaska_handle_t &out_handle) {
  out_handle = m_next_handle;
  m_next_handle += m_size;

  return translate(out_handle, AllocateIntermediate::Yes);
}