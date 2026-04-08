#include "./ArenaHeap.hpp"
#include <alaska/handles/HandleTable.hpp>
#include <sys/mman.h>
#include <stdio.h>

// check_mapping in HandleTable validates via Heap::get_page, which rejects arena pointers.
// This local variant skips that check — correctness is instead guaranteed by the
// `mapped_data == src->data()` test at each call site.
static bool arena_check_mapping(
    alaska::handle_id_t hid, alaska::Mapping *&out_m, void *&out_data) {
  void *handle = alaska::Mapping::handle_from_hid(hid);
  alaska::Mapping *m = alaska::Mapping::from_handle_safe(handle);
  if (m == nullptr || m > alaska::last_mapping) return false;
  if (not IS_WORD_ALIGNED(m)) return false;
  if (m->is_free()) return false;
  out_m   = m;
  out_data = m->get_pointer();
  return true;
}




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
    INIT_LIST_HEAD(&m_nursery);
    INIT_LIST_HEAD(&m_elderly);
  }

  void ArenaHeap::reset_age(ArenaBlock &block) {
    block.time_of_last_use = alaska::now_ms();
    list_del(&block.age_list);
    list_add(&block.age_list, &m_nursery);
  }

  void ArenaHeap::promote_to_elderly(ArenaBlock &block) {
    list_del(&block.age_list);
    list_add(&block.age_list, &m_elderly);
  }

  ArenaHeap::~ArenaHeap() {
    // Clean up all segments (which also cleans up their blocks).
    while (!list_empty(&segment_list)) {
      ArenaSegment *segment = list_entry(segment_list.next, ArenaSegment, segment_list);
      destroySegment(segment);
    }
  }

  void ArenaHeap::periodic_work(float deltaTime) {

    size_t total_compacted = 0;
    // for (int i = 0; i < bin_count - 1; i++) {
    //   const list_head *pos, *tmp;
    //   list_for_each_safe(pos, tmp, &bins[i]) {
    //     ArenaBlock *block = list_entry(pos, ArenaBlock, bin_list);
    //     total_compacted += block->compact();
    //   }
    // }

    size_t total_evacuated = evacuate();

    alaska::printf("Compacted %.2fmb, evacuated %.2fmb\n",
                   total_compacted / (1024.0f * 1024.0f),
                   total_evacuated / (1024.0f * 1024.0f));
    FILE *out = fopen("arenas", "w");
    this->dump(out);
    fclose(out);
  }

  size_t ArenaHeap::evacuate() {
    size_t reclaimed = 0;
    ArenaBlock *dst = nullptr;

    // Walk the most fragmented bins first (3 = 75-99% free, 2 = 50-75% free).
    for (int bin = bin_count - 2; bin >= 2; bin--) {
      alaska::printf("Evacuating bin %d\n", bin);
      list_head *pos, *tmp;
      list_for_each_safe(pos, tmp, &bins[bin]) {
        ArenaBlock *src_block = list_entry(pos, ArenaBlock, bin_list);

        // Skip blocks that still have allocatable space — a ThreadCache may hold
        // this block as active_block and would corrupt the destination if we reset bump.
        if (src_block->available() > 0) continue;

        char *const block_start = (char *)src_block->end - arena_size;
        char *const scan_end    = (char *)src_block->end;  // bump == end (verified above)
        char *read              = block_start;
        bool has_pinned         = false;

        while (read < scan_end) {
          auto *src         = (ObjectHeader *)read;
          size_t remaining  = (size_t)(scan_end - read);
          size_t obj_bytes  = src->real_object_size();

          if (__builtin_expect(obj_bytes < sizeof(ObjectHeader) || obj_bytes > remaining, 0)) break;

          alaska::Mapping *mapping  = nullptr;
          void *mapped_data         = nullptr;
          bool live = src->handle_id != 0 &&
                      arena_check_mapping(src->handle_id, mapping, mapped_data) &&
                      mapped_data == src->data();

          if (live) {
            if (mapping->is_pinned()) {
              has_pinned = true;
            } else {
              // Ensure the destination block has room.
              if (dst == nullptr || dst->available() < obj_bytes) {
                dst = newBlock();
              }
              src_block->freed_bytes += (uint32_t)obj_bytes;

              auto *dst_header = dst->allocate(src->size);
              memcpy(dst_header->data(), src->data(), src->object_size());
              dst_header->handle_id  = src->handle_id;
              dst_header->__metadata = src->__metadata;
              mapping->set_pointer(dst_header->data());
            }
          }

          read += obj_bytes;
        }

        if (!has_pinned) {
          // All live objects evacuated — return the source block to the ready pool.
          src_block->bump        = block_start;
          src_block->freed_bytes = 0;
          rebin(src_block, bin_count - 1);
          reclaimed += arena_size;
        }
      }
    }

    return reclaimed;
  }

  void ArenaHeap::dump(FILE *out) const {
    static const char *bin_labels[] = {
        "  full (<25% free)", "  75%  (25-50%)",  "  50%  (50-75%)",
        "  25%  (75-99%)",    "  empty (ready) ",
    };
    fprintf(out, "ArenaHeap @ %p\n", (void *)this);
    float total_mb_real = 0;
    float total_mb_used = 0;
    for (int i = 0; i < bin_count; i++) {
      int count = 0;
      const list_head *pos;
      size_t total_avail = 0;
      list_for_each(pos, &bins[i]) {
        const ArenaBlock *blk = list_entry(pos, ArenaBlock, bin_list);
        total_avail += blk->available();
        count++;
      }
      float megabytes_in_this_bin = (count * arena_size) / (1024.0f * 1024.0f);
      float megabytes_in_this_bin_avail = total_avail / (1024.0f * 1024.0f);
      float megabytes_in_this_bin_used = megabytes_in_this_bin - megabytes_in_this_bin_avail;
      total_mb_used += megabytes_in_this_bin;
      total_mb_real += megabytes_in_this_bin_used;
      fprintf(out, "  bin[%d] %20s : %6d blks   t:%8.2f   a:%8.2f   u:%8.2f\n", i, bin_labels[i], count,
              megabytes_in_this_bin, megabytes_in_this_bin_avail,
              megabytes_in_this_bin_used);
      if (count > 0) {
        list_for_each(pos, &bins[i]) {
          const ArenaBlock *blk = list_entry(pos, ArenaBlock, bin_list);
          float pct_free = 100.0f * blk->freed_bytes / (float)arena_size;
          // fprintf(out, "%3d ", (int)pct_free);
          // fprintf(out, "         %p  freed=%-6u  %f%%\n", (void *)blk, blk->freed_bytes,
          // pct_free);
        }
        // fprintf(out, "\n");
      }
    }

    float fragmentation = total_mb_used > 0 ? 100.0f * (total_mb_used - total_mb_real) / total_mb_used : 0.0f;
    fprintf(out, "Total arena heap usage: %.2fmb (frag=%8.2f%%)\n", total_mb_real, fragmentation);
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
    // Remove each block from the age lists before the memory is unmapped.
    for (ArenaBlock *block = segment->begin(); block != segment->end(); block++) {
      list_del(&block->age_list);
    }
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
      if (seg->num_blocks <= usable_arenas_per_segment) return seg;
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
      // Block already has an initialized age_list from its previous life; reset_age
      // will list_del + list_add safely.
      block->time_of_last_use = alaska::now_ms();
      list_del(&block->age_list);
      list_add(&block->age_list, &m_nursery);
      return block;
    }

    ArenaSegment *seg = ensureWritableSegment();
    if (seg == nullptr) return nullptr;

    ArenaBlock *block = seg->newBlock();
    if (block == nullptr) return nullptr;
    INIT_LIST_HEAD(&block->bin_list);
    block->current_bin = 0;
    list_add(&block->bin_list, &bins[0]);
    // age_list is already INIT'd by ArenaSegment::newBlock(); enroll in nursery.
    block->time_of_last_use = alaska::now_ms();
    list_add(&block->age_list, &m_nursery);
    return block;
  }



  bool ArenaHeap::contains(void *ptr) const {
    const list_head *pos;
    list_for_each(pos, &segment_list) {
      const ArenaSegment *seg = list_entry(pos, ArenaSegment, segment_list);
      uintptr_t start = (uintptr_t)seg;
      uintptr_t end = start + arena_segment_size;
      uintptr_t addr = (uintptr_t)ptr;
      if (addr >= start && addr < end) return true;
    }
    return false;
  }

  ArenaBlock *ArenaSegment::newBlock() {
    if (num_blocks > usable_arenas_per_segment) {
      // No more blocks can be allocated in this segment.
      return nullptr;
    }
    ArenaBlock *block = new (&blocks[num_blocks++]) ArenaBlock();
    block->bump = getArenaData(num_blocks - 1);
    block->end = (char *)block->bump + arena_size;
    INIT_LIST_HEAD(&block->age_list);
    return block;
  }

  ArenaSegment::ArenaSegment(ArenaHeap &heap)
      : owner(&heap) {
    INIT_LIST_HEAD(&this->segment_list);
    // The first block is technically spent since its the segment header + ArenaBlock headers, so we
    // skip it.
    this->num_blocks = 1;
  }


  size_t ArenaBlock::compact() {
    if (freed_bytes == 0) return 0;

    char *const block_start = (char *)end - arena_size;
    char *const scan_end = (char *)bump;

    char *read = block_start;
    char *write = block_start;

    while (read < scan_end) {
      auto *src = (ObjectHeader *)read;
      size_t remaining = (size_t)(scan_end - read);
      size_t object_bytes = src->real_object_size();

      if (__builtin_expect(object_bytes < sizeof(ObjectHeader) || object_bytes > remaining, 0)) {
        break;
      }

      alaska::Mapping *mapping = nullptr;
      void *mapped_data = nullptr;
      bool live = src->handle_id != 0 &&
                  arena_check_mapping(src->handle_id, mapping, mapped_data) &&
                  mapped_data == src->data();

      if (live) {
        if (mapping->is_pinned()) return 0;
        if (write != read) {
          memmove(write, read, object_bytes);
          auto *dst = (ObjectHeader *)write;
          mapping->set_pointer(dst->data());
        }
        write += object_bytes;
      }

      read += object_bytes;
    }

    size_t old_used = (size_t)(scan_end - block_start);
    size_t new_used = (size_t)(write - block_start);
    size_t reclaimed = old_used - new_used;

    bump = write;
    // freed_bytes = (uint32_t)(arena_size - new_used);
    freed_bytes = new_used == 0 ? (uint32_t)arena_size : 0;

    int new_bin = (int)(freed_bytes >> alaska::bin_shift);
    if (new_bin != (int)current_bin) {
      get_arena_segment(this)->owner->rebin(this, new_bin);
    }

    return reclaimed;
  }

}  // namespace alaska
