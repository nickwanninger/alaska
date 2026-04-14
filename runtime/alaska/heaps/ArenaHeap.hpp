/*
 * This file is part of the Alaska Handle-Based Memory Management System
 *
 * Copyright (c) 2024, Nick Wanninger <ncw@u.northwestern.edu>
 * Copyright (c) 2024, The Constellation Project
 * All rights reserved.
 *
 * This is free software.  You are permitted to use, redistribute,
 * and modify it as specified in the file "LICENSE".
 */

#pragma once

#include <alaska/heaps/Heap.hpp>
#include <alaska/heaps/SizeClass.hpp>
#include <ck/lock.h>
#include <string.h>
#include <alaska/work/WorkScheduler.hpp>
#include "alaska/alaska.hpp"

namespace alaska {



  static constexpr uint64_t arena_size_shift_factor = 16;
  static constexpr size_t arena_size = 1ULL << arena_size_shift_factor;

  static constexpr int    bin_count = 5;
  static constexpr int    bin_shift = arena_size_shift_factor - 2;  // 14
  static constexpr size_t bin_mask  = ~((arena_size / 4) - 1);      // ~0x3FFF

  static_assert(arena_size >= alaska::max_large_size * 8,
                "Arena size must be larger than the maximum large size.");


  struct ArenaSegment;
  struct ArenaBlock;



  class ArenaHeap : public alaska::Worker, public alaska::InternalHeapAllocated {
   public:
    ArenaHeap();
    virtual ~ArenaHeap();



    // Create a new segment.
    ArenaSegment *createSegment();
    void destroySegment(ArenaSegment *segment);

    // ThreadCache API: checkout acquires a block (marks it owned),
    // checkin releases it back (clears owned). Nothing else touches owned.
    ArenaBlock *checkout_block(void);
    void checkin_block(ArenaBlock *block);

    // Slab API: like checkout_block but stamps the block with a fixed size class.
    // The returned block allocates slots of class_to_size(cls) bytes each.
    ArenaBlock *checkout_slab_block(size_class_t cls);

    void rebin(ArenaBlock *block, int new_bin);
    bool contains(void *ptr) const;
    void dump(FILE *out = stderr) const;  // Debug Dump


    void periodic_work(float deltaTime) override; // ^Worker

    struct EvacStats {
      size_t reclaimed_bytes = 0;
      int candidates  = 0;  // blocks attempted
      int reclaimed   = 0;  // blocks reset to ready (bin 4)
      int skipped_avail  = 0;  // blocks skipped — still have allocatable space
      int skipped_pinned = 0;  // blocks stuck — at least one pinned object
    };
    EvacStats evacuate();  // Evacuate live objects from highly-fragmented blocks into fresh ones

    void reset_age(ArenaBlock &block);
    void promote_to_elderly(ArenaBlock &block);

    auto get_aging_blocks();

    template <typename Fn>
    void get_aging_blocks(uint64_t min_age_ms, Fn fn);

   private:
    ArenaBlock *newBlock(void);
    ArenaSegment *ensureWritableSegment();

    ck::mutex lock;
    struct list_head segment_list;
    struct list_head bins[bin_count];
    struct list_head slab_bins[alaska::num_size_classes];  // per-class available slab blocks
    struct list_head m_nursery;  // head=newest, tail=oldest
    struct list_head m_elderly;
  };


  struct ArenaBlock {
    void *bump;
    void *end;
    uint32_t freed_bytes = 0;
    uint32_t current_bin = 0;
    bool owned = false;  // true while a ThreadCache holds this as its active_block

    uint64_t time_of_last_use = 0;  // milliseconds, CLOCK_MONOTONIC
    struct list_head age_list;
    struct list_head bin_list;

    // Slab mode: when slab_class != 0, this block allocates fixed-size slots
    // of class_to_size(slab_class) bytes each. Freed slots are chained through
    // slab_free_list; the next pointer is stored in the first sizeof(void*)
    // bytes of the object data area (matching the SizedPage::SizePageBlock convention).
    size_class_t  slab_class     = 0;
    ObjectHeader *slab_free_list = nullptr;

    inline bool   is_slab() const { return slab_class != 0; }
    inline size_t slab_object_size() const { return alaska::class_to_size(slab_class); }
    inline size_t slab_slot_bytes() const { return slab_object_size() + sizeof(ObjectHeader); }
    inline size_t slab_total_slots() const { return arena_size / slab_slot_bytes(); }

    ObjectHeader *allocate(AlignedSize size);
    void free(ObjectHeader *header);
    size_t compact();
    inline void reset() {
      bump = (char *)end - arena_size;
      freed_bytes = 0;
      slab_class = 0;
      slab_free_list = nullptr;
    }
    inline size_t available() const { return (char *)end - (char *)bump; }
    inline size_t used() const { return alaska::arena_size - available(); }
  };


  // A segment is a contiguous memory block aligned to the segment
  // size/alignment. It contains multiple arenas.
  struct ArenaSegment {
    ArenaHeap *owner;
    struct list_head segment_list;
    uint32_t num_blocks;  // Bump allocator for blocks in this segment.
    ArenaBlock blocks[];


    void *getArenaData(size_t index) { return (char *)this + index * arena_size; }


    ArenaBlock *newBlock();
    ArenaBlock *getBlock(size_t index) { return &blocks[index]; }


    ArenaBlock *begin() { return &blocks[1]; }
    ArenaBlock *end() { return &blocks[num_blocks]; }

   protected:
    friend class ArenaHeap;
    ArenaSegment(ArenaHeap &heap);
  };
  static constexpr size_t arena_segment_size = 1 << 24;

  // Subtract one for the segment header and one for the first block which is reserved for metadata.

  static constexpr size_t header_capacity   = (arena_size - sizeof(ArenaSegment)) / sizeof(ArenaBlock);
  static constexpr size_t physical_capacity = arena_segment_size / arena_size;

  static_assert(header_capacity >= physical_capacity,
      "First arena cannot hold headers for all physical arenas — "
      "increase arena_size or reduce arena_segment_size.");

  static constexpr size_t usable_arenas_per_segment =
      (header_capacity < physical_capacity ? header_capacity : physical_capacity) - 1;


  // Given any pointer, return the base address of the arena segment it belongs to.
  template <typename T>
  static inline ArenaSegment *get_arena_segment(T *ptr) {
    return (ArenaSegment *)((uintptr_t)ptr & ~(arena_segment_size - 1));
  }

  // Given any pointer into arena memory, return the ArenaBlock that owns it.
  static inline ArenaBlock *get_arena_block(void *ptr) {
    ArenaSegment *seg = get_arena_segment(ptr);
    size_t idx = ((uintptr_t)ptr - (uintptr_t)seg) >> arena_size_shift_factor;
    return seg->getBlock(idx);
  }


  inline ObjectHeader *ArenaBlock::allocate(AlignedSize size) {
    if (slab_class != 0) {
      if (slab_free_list != nullptr) {
        ObjectHeader *h = slab_free_list;
        slab_free_list = *reinterpret_cast<ObjectHeader **>(h->data());
        h->size = size;
        freed_bytes -= (uint32_t)slab_slot_bytes();
        return h;
      }
      // No recycled slots — try to bump-allocate a fresh slot.
      size_t total_size = sizeof(ObjectHeader) + size;
      void *new_bump = (char *)bump + total_size;
      if (new_bump > (char *)end) {
        // The remaining tail bytes are alignment waste; don't account for them.
        return nullptr;
      }
      ObjectHeader *header = (ObjectHeader *)bump;
      header->size = size;
      bump = new_bump;
      return header;
    }

    size_t total_size = sizeof(ObjectHeader) + size;

    void *new_bump = (char *)bump + total_size;
    if (new_bump > (char *)end) {
      uint32_t old = freed_bytes;
      freed_bytes += available();
      bump = end;
      if (__builtin_expect((old ^ freed_bytes) & alaska::bin_mask, 0)) {
        int new_bin = (int)(freed_bytes >> alaska::bin_shift);
        get_arena_segment(this)->owner->rebin(this, new_bin);
      }
      return nullptr;
    }

    ObjectHeader *header = (ObjectHeader *)bump;
    header->size = size;
    bump = new_bump;
    return header;
  }

  inline void ArenaBlock::free(ObjectHeader *header) {
    if (slab_class != 0) {
      header->handle_id = 0;
      *reinterpret_cast<ObjectHeader **>(header->data()) = slab_free_list;
      slab_free_list = header;
      freed_bytes += (uint32_t)slab_slot_bytes();
      return;
    }

    uint32_t old = freed_bytes;
    freed_bytes += header->real_object_size();

    if (__builtin_expect((old ^ freed_bytes) & alaska::bin_mask, 0)) {
      int new_bin = (int)(freed_bytes >> alaska::bin_shift);
      get_arena_segment(this)->owner->rebin(this, new_bin);
    }
  }

  inline auto ArenaHeap::get_aging_blocks() {
    return alaska::list_range_reverse<ArenaBlock, &ArenaBlock::age_list>(&m_nursery);
  }

  template <typename Fn>
  inline void ArenaHeap::get_aging_blocks(uint64_t min_age_ms, Fn fn) {
    auto cutoff = alaska::now_ms() - min_age_ms;
    for (auto *block : get_aging_blocks()) {
      if (block->time_of_last_use > cutoff) break;
      fn(block);
    }
  }

}  // namespace alaska
