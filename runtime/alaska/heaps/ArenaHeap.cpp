#include "./ArenaHeap.hpp"
#include <sys/mman.h>




// This feels like overkill, but we want to ensure that memory is aligned to a 2MB boundary for
// optimal performance with
static void *mmap_aligned_2mb(size_t size, size_t alignment) {
  // Round size up to a 2MB multiple
  size_t aligned_size = (size + alignment - 1) & ~(alignment - 1);

  // Over-allocate by one full alignment worth so we can always find
  // an aligned start within the region
  size_t total = aligned_size + alignment;

  void *raw =
      mmap(NULL, total, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
  if (raw == MAP_FAILED) return NULL;

  // Find the next 2MB-aligned address within the mapping
  uintptr_t raw_addr = (uintptr_t)raw;
  uintptr_t aligned_addr = (raw_addr + alignment - 1) & ~(alignment - 1);

  // Trim the prefix (bytes before the aligned start)
  size_t prefix = aligned_addr - raw_addr;
  if (prefix > 0) munmap(raw, prefix);

  // Trim the suffix (bytes after the aligned region)
  size_t suffix = alignment - prefix;
  if (suffix > 0) munmap((void *)(aligned_addr + aligned_size), suffix);

  return (void *)aligned_addr;
}

namespace alaska {

  ArenaHeap::ArenaHeap() { INIT_LIST_HEAD(&this->segment_list); }


  ArenaSegment *ArenaHeap::createSegment(void) {
    void *arena = mmap_aligned_2mb(alaska::arena_segment_size, alaska::arena_segment_size);
    if (arena == NULL) return NULL;

    ArenaSegment *segment = new (arena) ArenaSegment(*this);

    // Add the new segment to the heap's list of segments.
    list_add(&segment->segment_list, &this->segment_list);
    return segment;
  }

  void ArenaHeap::destroySegment(ArenaSegment *segment) {
    // Remove the segment from the heap's list of segments.
    list_del(&segment->segment_list);
    // Unmap the memory used by the segment.
    munmap(segment, alaska::arena_segment_size);
  }

  ArenaBlock *ArenaHeap::newBlock() {
    // Try to allocate a new block from the most recently created segment.
    // TODO: arena tiers!

    if (list_empty(&this->segment_list)) {
      // No segments exist yet, so we need to create one.
      ArenaSegment *new_segment = this->createSegment();
      if (new_segment == nullptr) {
        return nullptr;  // Failed to create a new segment.
      }
    }

    ArenaSegment *last_segment = list_entry(this->segment_list.prev, ArenaSegment, segment_list);
    ArenaBlock *block = last_segment->newBlock();
    if (block != nullptr) {
      return block;
    }

    return nullptr;
  }



  ArenaBlock *ArenaSegment::newBlock() {
    if (num_blocks >= max_arenas_per_segment) {
      return nullptr;  // No more blocks can be allocated in this segment.
    }
    ArenaBlock *block = new (&blocks[num_blocks++]) ArenaBlock();
    // TODO: This should link the block into some global list of blocks.
    block->bump = getArenaData(num_blocks - 1);
    block->end = (char *)block->bump + arena_size;
    return block;
  }

  ArenaSegment::ArenaSegment(ArenaHeap &heap)
      : owner(&heap) {
    INIT_LIST_HEAD(&this->segment_list);
    // The first block is technically spent since its the segment header + ArenaBlock headers, so we
    // skip it.
    this->num_blocks = 1;
  }

}  // namespace alaska