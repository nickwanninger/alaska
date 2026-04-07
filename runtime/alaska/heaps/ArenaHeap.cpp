#include "./ArenaHeap.hpp"
#include <sys/mman.h>
#include <stdio.h>




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

  ArenaHeap::ArenaHeap() {
    INIT_LIST_HEAD(&this->segment_list);
    for (auto &b : bins)
      INIT_LIST_HEAD(&b);
  }

  ArenaHeap::~ArenaHeap() {
    dump(stderr);
    // Clean up all segments (which also cleans up their blocks).
    while (!list_empty(&segment_list)) {
      ArenaSegment *segment = list_entry(segment_list.next, ArenaSegment, segment_list);
      destroySegment(segment);
    }
  }

  void ArenaHeap::dump(FILE *out) const {
    static const char *bin_labels[] = {
        "  full (<25% free)", "  75%  (25-50%)",  "  50%  (50-75%)",
        "  25%  (75-99%)",    "  empty (ready) ",
    };
    fprintf(out, "ArenaHeap @ %p\n", (void *)this);
    for (int i = 0; i < bin_count; i++) {
      int count = 0;
      const list_head *pos;
      list_for_each(pos, &bins[i]) count++;
      float megabytes_in_this_bin = (count * arena_size) / (1024.0f * 1024.0f);
      fprintf(out, "  bin[%d] %s : %d block(s) (%.2fmb)\n", i, bin_labels[i], count,
              megabytes_in_this_bin);
      if (count > 0) {
        list_for_each(pos, &bins[i]) {
          const ArenaBlock *blk = list_entry(pos, ArenaBlock, bin_list);
          float pct_free = 100.0f * blk->freed_bytes / (float)arena_size;
          fprintf(out, "         %p  freed=%-6u  %f%%\n", (void *)blk, blk->freed_bytes, pct_free);
        }
      }
    }
  }

  void ArenaHeap::rebin(ArenaBlock *block, int new_bin) {
    ck::scoped_lock lk(lock);
    list_del(&block->bin_list);
    block->current_bin = new_bin;
    list_add(&block->bin_list, &bins[new_bin]);
  }


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

  // Returns a segment with available block capacity, creating one if needed.
  // Must be called with the heap lock held.
  ArenaSegment *ArenaHeap::ensureWritableSegment() {
    if (!list_empty(&segment_list)) {
      auto *seg = list_entry(segment_list.next, ArenaSegment, segment_list);
      if (seg->num_blocks < usable_arenas_per_segment) return seg;
    }
    return createSegment();
  }

  ArenaBlock *ArenaHeap::newBlock() {
    ck::scoped_lock lk(lock);

    // Prefer reusing a block that has been fully emptied.
    if (!list_empty(&bins[4])) {
      ArenaBlock *block = list_entry(bins[4].next, ArenaBlock, bin_list);
      block->reset();
      // Rebin inline — lock already held, don't call rebin() to avoid re-locking.
      list_del(&block->bin_list);
      block->current_bin = 0;
      list_add(&block->bin_list, &bins[0]);
      return block;
    }

    ArenaSegment *seg = ensureWritableSegment();
    if (seg == nullptr) return nullptr;

    ArenaBlock *block = seg->newBlock();
    if (block == nullptr) return nullptr;
    INIT_LIST_HEAD(&block->bin_list);
    block->current_bin = 0;
    list_add(&block->bin_list, &bins[0]);
    return block;
  }



  ArenaBlock *ArenaSegment::newBlock() {
    if (num_blocks >= usable_arenas_per_segment) {
      // No more blocks can be allocated in this segment.
      return nullptr;
    }
    ArenaBlock *block = new (&blocks[num_blocks++]) ArenaBlock();
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