#include <gtest/gtest.h>
#include "gtest/gtest.h"
#include <gmock/gmock.h>

#include <vector>
#include <alaska/heaps/ArenaHeap.hpp>
#include <alaska/core/Runtime.hpp>
#include <alaska.h>

using namespace alaska;

class ArenaHeapTest : public ::testing::Test {
 public:
  void SetUp() override { this->seg = heap.createSegment(); }
  void TearDown() override { heap.destroySegment(this->seg); }


  alaska::ArenaHeap heap;

  alaska::ArenaSegment *seg;
};


TEST_F(ArenaHeapTest, SegmentAllocationIsAligned) {
  // Segment must be aligned to alaska::arena_segment_size
  EXPECT_EQ((uintptr_t)seg, (uintptr_t)seg & ~(arena_segment_size - 1));
}


TEST_F(ArenaHeapTest, SegmentFirstAllocationIsSane) {
  // First allocation should be right after the header
  ArenaBlock *first_alloc = seg->newBlock();
  EXPECT_EQ(first_alloc, seg->getBlock(1));
}


TEST_F(ArenaHeapTest, SegmentAllocationIncreasesOffset) {
  // Each allocation should increase the offset by the size of the block
  ArenaBlock *first_alloc = seg->newBlock();
  ArenaBlock *second_alloc = seg->newBlock();
  EXPECT_GT((uintptr_t)second_alloc, (uintptr_t)first_alloc);
}


TEST_F(ArenaHeapTest, BlockCountIsCorrect) {
  // Expect arenas_per_segment
  std::vector<ArenaBlock *> blocks;
  while (true) {
    auto *blk = seg->newBlock();
    if (blk == nullptr) break;  // No more space in the segment
    blocks.push_back(blk);
  }
  EXPECT_EQ(blocks.size(), usable_arenas_per_segment);
}


TEST_F(ArenaHeapTest, IterationOrder) {
  // Allocating multiple blocks and iterating should yield them in order
  std::vector<ArenaBlock *> blocks;
  while (true) {
    auto *blk = seg->newBlock();
    if (blk == nullptr) break;  // No more space in the segment
    blocks.push_back(blk);
  }
  size_t index = 0;
  for (ArenaBlock &block : *seg) {
    EXPECT_EQ(&block, blocks[index]);
    index++;
  }
}


TEST_F(ArenaHeapTest, MinimumObjectAllocation) {
  // Each block should have the correct size
  ArenaBlock *blk = seg->newBlock();

  size_t alloc_req = alaska::alignment;  // Must be aligned; AlignedSize rounds up to 16
  auto *object = blk->allocate(alloc_req);
  EXPECT_NE(object, nullptr);
  EXPECT_EQ(object->object_size(), alloc_req);
}


TEST_F(ArenaHeapTest, AvailableDrops) {
  // Each block should have the correct size
  ArenaBlock *blk = seg->newBlock();

  size_t initial_available = blk->available();

  size_t alloc_req = alaska::alignment;  // Must be aligned; AlignedSize rounds up to 16
  auto *object = blk->allocate(alloc_req);
  size_t after_available = blk->available();
  EXPECT_NE(initial_available, after_available);

  EXPECT_EQ(initial_available - after_available, alloc_req + sizeof(ObjectHeader));
}


// ─── Bin system tests ────────────────────────────────────────────────────────
//
// These tests use heap.checkout_block() (not seg->newBlock()) so that blocks are
// properly enrolled in the bin system from the start.

class ArenaBinTest : public ::testing::Test {
 public:
  alaska::ArenaHeap heap;

  // Allocate an object whose real_object_size() equals exactly `real_size`.
  // NOTE: real_size - sizeof(ObjectHeader) must already be a multiple of alaska::alignment.
  ObjectHeader *alloc_real(ArenaBlock *blk, size_t real_size) {
    size_t obj_size = real_size - sizeof(ObjectHeader);
    return blk->allocate(obj_size);
  }

  // Allocate the largest aligned object that fits, then exhaust trailing slack bytes
  // via a failing allocation. Afterwards blk->freed_bytes == 0 but the block is full.
  // Call blk->free(result) to drive freed_bytes to arena_size and land in bin 4.
  ObjectHeader *fill_block(ArenaBlock *blk) {
    size_t max_data = (blk->available() - sizeof(ObjectHeader)) & ~(alaska::alignment - 1);
    ObjectHeader *obj = blk->allocate(max_data);
    if (obj == nullptr) return nullptr;
    blk->allocate(alaska::alignment);  // Fails → freed_bytes absorbs the trailing bytes
    return obj;
  }
};


TEST_F(ArenaBinTest, NewBlockStartsInBin0) {
  ArenaBlock *blk = heap.checkout_block();
  ASSERT_NE(blk, nullptr);
  EXPECT_EQ(blk->current_bin, 0);
}


TEST_F(ArenaBinTest, QuarterCrossingMovesToBin1) {
  ArenaBlock *blk = heap.checkout_block();
  ASSERT_NE(blk, nullptr);
  EXPECT_EQ(blk->current_bin, 0);

  // Free exactly arena_size/4 bytes to cross into bin 1.
  size_t quarter = arena_size / 4;
  ObjectHeader *obj = alloc_real(blk, quarter);
  ASSERT_NE(obj, nullptr);

  blk->free(obj);
  EXPECT_EQ(blk->current_bin, 1);
}


TEST_F(ArenaBinTest, MultiSkipLandsInCorrectBin) {
  ArenaBlock *blk = heap.checkout_block();
  ASSERT_NE(blk, nullptr);

  // A single free worth 3/4 of the arena skips from bin 0 directly to bin 3.
  size_t three_quarters = (arena_size / 4) * 3;
  ObjectHeader *obj = alloc_real(blk, three_quarters);
  ASSERT_NE(obj, nullptr);

  blk->free(obj);
  EXPECT_EQ(blk->current_bin, 3);
}


TEST_F(ArenaBinTest, FullBlockReachesReadyBin) {
  ArenaBlock *blk = heap.checkout_block();
  ASSERT_NE(blk, nullptr);

  // Fill the entire block and free it — freed_bytes reaches arena_size → bin 4.
  ObjectHeader *obj = fill_block(blk);
  ASSERT_NE(obj, nullptr);

  blk->free(obj);
  EXPECT_EQ(blk->current_bin, 4);
}


TEST_F(ArenaBinTest, NewBlockPrefersReadyBin) {
  // Get a block into bin 4.
  ArenaBlock *blk = heap.checkout_block();
  ASSERT_NE(blk, nullptr);
  ObjectHeader *obj = fill_block(blk);
  ASSERT_NE(obj, nullptr);
  blk->free(obj);
  ASSERT_EQ(blk->current_bin, 4);

  // newBlock() should hand back the same block, not allocate a fresh one.
  ArenaBlock *reused = heap.checkout_block();
  EXPECT_EQ(reused, blk);
}


TEST_F(ArenaBinTest, ReusedBlockIsReset) {
  ArenaBlock *blk = heap.checkout_block();
  ASSERT_NE(blk, nullptr);
  ObjectHeader *obj = fill_block(blk);
  ASSERT_NE(obj, nullptr);
  blk->free(obj);
  ASSERT_EQ(blk->current_bin, 4);

  ArenaBlock *reused = heap.checkout_block();
  EXPECT_EQ(reused->freed_bytes, 0u);
  EXPECT_EQ(reused->available(), arena_size);
  EXPECT_EQ(reused->current_bin, 0);
}


TEST_F(ArenaBinTest, NoSpuriousRebinBelowThreshold) {
  ArenaBlock *blk = heap.checkout_block();
  ASSERT_NE(blk, nullptr);

  // Free many tiny objects; total stays well below arena_size/4.
  // Minimum object = alignment(16) bytes data + 8 bytes header = 24 bytes real.
  // Loop while freed_total + real_size < quarter boundary (16384).
  size_t freed_total = 0;
  size_t quarter = arena_size / 4;
  size_t obj_real = alaska::alignment + sizeof(ObjectHeader);
  while (freed_total + obj_real < quarter) {
    ObjectHeader *obj = blk->allocate(alaska::alignment);
    ASSERT_NE(obj, nullptr);
    blk->free(obj);
    freed_total += obj_real;
  }
  EXPECT_EQ(blk->current_bin, 0);
}


TEST_F(ArenaBinTest, ExactBoundaryTransitions) {
  // Verify each quarter boundary triggers the right bin transition.
  // With 8-byte headers and 16-byte alignment, an object with data_size=16376
  // has AlignedSize=16384 and real_object_size=16392. Three of these fit in the block
  // (3*16392=49176), and a fourth with data_size=16352 (real=16360) fills the rest
  // (49176+16360=65536=arena_size). Each free crosses exactly one bin boundary.
  ArenaBlock *blk = heap.checkout_block();
  ASSERT_NE(blk, nullptr);

  ObjectHeader *o1 = blk->allocate(16376);  // real = 16392; freed → 16392 → bin 1
  ASSERT_NE(o1, nullptr);
  blk->free(o1);
  EXPECT_EQ(blk->current_bin, 1);

  ObjectHeader *o2 = blk->allocate(16376);  // freed → 32784 → bin 2
  ASSERT_NE(o2, nullptr);
  blk->free(o2);
  EXPECT_EQ(blk->current_bin, 2);

  ObjectHeader *o3 = blk->allocate(16376);  // freed → 49176 → bin 3
  ASSERT_NE(o3, nullptr);
  blk->free(o3);
  EXPECT_EQ(blk->current_bin, 3);

  ObjectHeader *o4 = blk->allocate(16352);  // real = 16360; freed → 65536 → bin 4
  ASSERT_NE(o4, nullptr);
  blk->free(o4);
  EXPECT_EQ(blk->current_bin, 4);
}




// ─── Slab block tests ────────────────────────────────────────────────────────

class ArenaSlabTest : public ::testing::Test {
 public:
  alaska::ArenaHeap heap;
};


// A slab block for size class 1 (16-byte objects) starts with slab_class set,
// owned, and has a fresh bump pointer ready for slot allocation.
TEST_F(ArenaSlabTest, CheckoutSlabBlockIsStamped) {
  size_class_t cls = alaska::size_to_class(16);
  ArenaBlock *blk = heap.checkout_slab_block(cls);
  ASSERT_NE(blk, nullptr);
  EXPECT_TRUE(blk->is_slab());
  EXPECT_EQ(blk->slab_class, cls);
  EXPECT_TRUE(blk->owned);
  EXPECT_EQ(blk->slab_free_list, nullptr);
  heap.checkin_block(blk);
}


// Allocating one slot advances bump and returns a properly-sized header.
TEST_F(ArenaSlabTest, SlabAllocReturnsSizedHeader) {
  size_class_t cls = alaska::size_to_class(32);
  size_t obj_size = alaska::class_to_size(cls);  // 32

  ArenaBlock *blk = heap.checkout_slab_block(cls);
  ASSERT_NE(blk, nullptr);

  void *bump_before = blk->bump;
  ObjectHeader *h = blk->allocate(obj_size);
  ASSERT_NE(h, nullptr);
  EXPECT_EQ(h->object_size(), obj_size);
  EXPECT_GT(blk->bump, bump_before);  // bump advanced

  heap.checkin_block(blk);
}


// After filling all slots and freeing each one, freed_bytes equals the number
// of slots times slot_bytes. The free list chains all freed slots.
TEST_F(ArenaSlabTest, SlabAllocFreeRoundTrip) {
  size_class_t cls = alaska::size_to_class(64);
  size_t obj_size = alaska::class_to_size(cls);  // 64

  ArenaBlock *blk = heap.checkout_slab_block(cls);
  ASSERT_NE(blk, nullptr);

  size_t slot_bytes = blk->slab_slot_bytes();
  size_t total_slots = blk->slab_total_slots();

  // Allocate every slot.
  std::vector<ObjectHeader *> headers;
  while (true) {
    ObjectHeader *h = blk->allocate(obj_size);
    if (h == nullptr) break;
    headers.push_back(h);
  }
  // Tail bytes (arena_size % slot_bytes) are wasted padding — not accounted in freed_bytes.
  EXPECT_EQ(headers.size(), total_slots);
  EXPECT_EQ(blk->freed_bytes, 0u);

  // Free every slot.
  for (auto *h : headers)
    blk->free(h);

  // freed_bytes tracks slot-sized chunks, not the tail padding.
  EXPECT_EQ(blk->freed_bytes, (uint32_t)(total_slots * slot_bytes));
  EXPECT_NE(blk->slab_free_list, nullptr);  // free list is populated

  heap.checkin_block(blk);
}


// After filling all bump slots and freeing half of them, allocating that same
// count must come from the free list (bump pointer must not advance).
TEST_F(ArenaSlabTest, SlabFreeListReuse) {
  size_class_t cls = alaska::size_to_class(128);
  size_t obj_size = alaska::class_to_size(cls);

  ArenaBlock *blk = heap.checkout_slab_block(cls);
  ASSERT_NE(blk, nullptr);

  // Fill.
  std::vector<ObjectHeader *> headers;
  while (true) {
    ObjectHeader *h = blk->allocate(obj_size);
    if (h == nullptr) break;
    headers.push_back(h);
  }
  ASSERT_FALSE(headers.empty());

  // Free every other slot.
  size_t freed_count = 0;
  for (size_t i = 0; i < headers.size(); i += 2) {
    blk->free(headers[i]);
    freed_count++;
  }
  EXPECT_EQ(blk->freed_bytes, (uint32_t)(freed_count * blk->slab_slot_bytes()));

  void *bump_snapshot = blk->bump;

  // Re-allocate the freed count — must all come from the free list.
  for (size_t i = 0; i < freed_count; i++) {
    ObjectHeader *h = blk->allocate(obj_size);
    ASSERT_NE(h, nullptr);
  }

  // Bump pointer must not have moved.
  EXPECT_EQ(blk->bump, bump_snapshot);
  EXPECT_EQ(blk->freed_bytes, 0u);

  heap.checkin_block(blk);
}


// A checked-in slab block is placed in slab_bins; a second checkout_slab_block
// for the same class returns the same block (not a fresh one).
TEST_F(ArenaSlabTest, CheckinRestoresBlockToSlabBin) {
  size_class_t cls = alaska::size_to_class(16);
  ArenaBlock *blk = heap.checkout_slab_block(cls);
  ASSERT_NE(blk, nullptr);

  heap.checkin_block(blk);
  EXPECT_FALSE(blk->owned);

  ArenaBlock *same = heap.checkout_slab_block(cls);
  EXPECT_EQ(same, blk);

  heap.checkin_block(same);
}


// Slab blocks and mixed blocks can coexist in the same ArenaHeap.
// get_arena_block() must return the correct block for pointers from each.
TEST_F(ArenaSlabTest, SlabAndMixedBlocksCoexist) {
  size_class_t cls = alaska::size_to_class(32);
  size_t obj_size = alaska::class_to_size(cls);

  ArenaBlock *slab = heap.checkout_slab_block(cls);
  ArenaBlock *mixed = heap.checkout_block();
  ASSERT_NE(slab, nullptr);
  ASSERT_NE(mixed, nullptr);
  EXPECT_NE(slab, mixed);

  ObjectHeader *sh = slab->allocate(obj_size);
  ObjectHeader *mh = mixed->allocate(16);
  ASSERT_NE(sh, nullptr);
  ASSERT_NE(mh, nullptr);

  EXPECT_EQ(get_arena_block(sh->data()), slab);
  EXPECT_EQ(get_arena_block(mh->data()), mixed);

  heap.checkin_block(slab);
  heap.checkin_block(mixed);
}


// When a slab block is checked in and its slab_class is cleared via reset(),
// checkout_block() can recycle it as a plain mixed block.
TEST_F(ArenaSlabTest, RecycledSlabBlockBecomesCleanMixedBlock) {
  size_class_t cls = alaska::size_to_class(16);
  size_t obj_size = alaska::class_to_size(cls);

  ArenaBlock *slab = heap.checkout_slab_block(cls);
  ASSERT_NE(slab, nullptr);

  // Fill and free everything so freed_bytes is maximal.
  std::vector<ObjectHeader *> headers;
  while (true) {
    ObjectHeader *h = slab->allocate(obj_size);
    if (h == nullptr) break;
    headers.push_back(h);
  }
  for (auto *h : headers)
    slab->free(h);

  // Manually reset to make it a fully-reclaimed mixed block.
  slab->reset();
  EXPECT_FALSE(slab->is_slab());
  EXPECT_EQ(slab->freed_bytes, 0u);
  EXPECT_EQ(slab->slab_free_list, nullptr);
  EXPECT_EQ(slab->available(), arena_size);
}


// Slab blocks must not appear in the evacuation candidates because they live in
// slab_bins[], not in the mixed fragmentation bins[].
TEST_F(ArenaSlabTest, SlabBlocksSkippedByEvacuation) {
  size_class_t cls = alaska::size_to_class(64);
  size_t obj_size = alaska::class_to_size(cls);

  ArenaBlock *blk = heap.checkout_slab_block(cls);
  ASSERT_NE(blk, nullptr);

  // Fill then free 80% of slots.
  std::vector<ObjectHeader *> headers;
  while (true) {
    ObjectHeader *h = blk->allocate(obj_size);
    if (h == nullptr) break;
    headers.push_back(h);
  }
  size_t to_free = headers.size() * 4 / 5;
  for (size_t i = 0; i < to_free; i++)
    blk->free(headers[i]);

  heap.checkin_block(blk);

  auto stats = heap.evacuate();
  EXPECT_EQ(stats.candidates, 0);
}
