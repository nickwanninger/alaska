#pragma once

#include <alaska/heaps/Heap.hpp>
#include <alaska/heaps/SizeClass.hpp>

namespace alaska {


  static constexpr size_t arena_segment_size = 1 << 24;

  static constexpr uint64_t arena_size_shift_factor = 16;
  static constexpr size_t arena_size = 1ULL << arena_size_shift_factor;

  static_assert(arena_size >= alaska::max_large_size * 8,
                "Arena size must be larger than the maximum large size.");


  struct ArenaSegment;
  struct ArenaBlock;



  class ArenaHeap {
   public:
    ArenaHeap();



    // Create a new segment.
    ArenaSegment *createSegment();
    void destroySegment(ArenaSegment *segment);

    ArenaBlock *newBlock(void);



   private:
    struct list_head segment_list;
  };


  struct ArenaBlock {
    struct list_head age_list;
    uint32_t freed_bytes = 0;
    void *bump;
    void *end;

    ObjectHeader *allocate(size_t size);
    void free(ObjectHeader *header);
    inline void reset() { bump = (char *)end - arena_size; }
    inline size_t available() { return (char *)end - (char *)bump; }
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


  // Subtract one for the segment header and one for the first block which is reserved for metadata.
  static constexpr uint64_t max_arenas_per_segment =
      (arena_size - sizeof(ArenaSegment)) / sizeof(ArenaSegment);
  static constexpr uint64_t usable_arenas_per_segment = max_arenas_per_segment - 1;



  // Given any pointer, return the base address of the arena segment it belongs to.
  template <typename T>
  static inline ArenaSegment *get_arena_segment(T *ptr) {
    return (ArenaSegment *)((uintptr_t)ptr & ~(arena_segment_size - 1));
  }


  inline ObjectHeader *ArenaBlock::allocate(size_t size) {
    size_t total_size = sizeof(ObjectHeader) + size;

    void *new_bump = (char *)bump + total_size;
    if (new_bump > (char *)end) {
      return nullptr;  // Not enough space in this block.
    }

    ObjectHeader *header = (ObjectHeader *)bump;
    header->size = size;
    bump = new_bump;
    return header;
  }

  inline void ArenaBlock::free(ObjectHeader *header) {
    // In a bump allocator, we don't actually free individual objects.
    // Instead, we can only reset the entire block when all objects are freed.
    // For simplicity, we won't implement reference counting here. The caller
    // is responsible for resetting the block when it's no longer needed.
    freed_bytes += header->real_object_size();
  }

}  // namespace alaska