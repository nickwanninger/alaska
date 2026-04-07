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

  size_t alloc_req = 8;
  auto *object = blk->allocate(alloc_req);
  EXPECT_NE(object, nullptr);
  EXPECT_EQ(object->object_size(), alloc_req);
}


TEST_F(ArenaHeapTest, AvailableDrops) {
  // Each block should have the correct size
  ArenaBlock *blk = seg->newBlock();

  size_t initial_available = blk->available();

  size_t alloc_req = 8;
  auto *object = blk->allocate(alloc_req);
  size_t after_available = blk->available();
  EXPECT_NE(initial_available, after_available);

  EXPECT_EQ(initial_available - after_available, alloc_req + sizeof(ObjectHeader));
}


// ─── Bin system tests ────────────────────────────────────────────────────────
//
// These tests use heap.newBlock() (not seg->newBlock()) so that blocks are
// properly enrolled in the bin system from the start.

class ArenaBinTest : public ::testing::Test {
 public:
  alaska::ArenaHeap heap;

  // Allocate an object whose real_object_size() equals exactly `real_size`.
  ObjectHeader *alloc_real(ArenaBlock *blk, size_t real_size) {
    size_t obj_size = real_size - sizeof(ObjectHeader);
    return blk->allocate(obj_size);
  }
};


TEST_F(ArenaBinTest, NewBlockStartsInBin0) {
  ArenaBlock *blk = heap.newBlock();
  ASSERT_NE(blk, nullptr);
  EXPECT_EQ(blk->current_bin, 0);
}


TEST_F(ArenaBinTest, QuarterCrossingMovesToBin1) {
  ArenaBlock *blk = heap.newBlock();
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
  ArenaBlock *blk = heap.newBlock();
  ASSERT_NE(blk, nullptr);

  // A single free worth 3/4 of the arena skips from bin 0 directly to bin 3.
  size_t three_quarters = (arena_size / 4) * 3;
  ObjectHeader *obj = alloc_real(blk, three_quarters);
  ASSERT_NE(obj, nullptr);

  blk->free(obj);
  EXPECT_EQ(blk->current_bin, 3);
}


TEST_F(ArenaBinTest, FullBlockReachesReadyBin) {
  ArenaBlock *blk = heap.newBlock();
  ASSERT_NE(blk, nullptr);

  // Fill the entire block and free it — freed_bytes reaches arena_size → bin 4.
  ObjectHeader *obj = alloc_real(blk, arena_size);
  ASSERT_NE(obj, nullptr);

  blk->free(obj);
  EXPECT_EQ(blk->current_bin, 4);
}


TEST_F(ArenaBinTest, NewBlockPrefersReadyBin) {
  // Get a block into bin 4.
  ArenaBlock *blk = heap.newBlock();
  ASSERT_NE(blk, nullptr);
  ObjectHeader *obj = alloc_real(blk, arena_size);
  ASSERT_NE(obj, nullptr);
  blk->free(obj);
  ASSERT_EQ(blk->current_bin, 4);

  // newBlock() should hand back the same block, not allocate a fresh one.
  ArenaBlock *reused = heap.newBlock();
  EXPECT_EQ(reused, blk);
}


TEST_F(ArenaBinTest, ReusedBlockIsReset) {
  ArenaBlock *blk = heap.newBlock();
  ASSERT_NE(blk, nullptr);
  ObjectHeader *obj = alloc_real(blk, arena_size);
  ASSERT_NE(obj, nullptr);
  blk->free(obj);
  ASSERT_EQ(blk->current_bin, 4);

  ArenaBlock *reused = heap.newBlock();
  EXPECT_EQ(reused->freed_bytes, 0u);
  EXPECT_EQ(reused->available(), arena_size);
  EXPECT_EQ(reused->current_bin, 0);
}


TEST_F(ArenaBinTest, NoSpuriousRebinBelowThreshold) {
  ArenaBlock *blk = heap.newBlock();
  ASSERT_NE(blk, nullptr);

  // Free many tiny objects; total stays well below arena_size/4.
  // Each alloc_real(blk, 9) → real_object_size = 9 bytes.
  // 1000 frees → 9000 bytes freed, which is < 16384 (quarter boundary).
  size_t freed_total = 0;
  size_t quarter = arena_size / 4;
  while (freed_total + 9 < quarter) {
    ObjectHeader *obj = alloc_real(blk, 9);
    ASSERT_NE(obj, nullptr);
    blk->free(obj);
    freed_total += 9;
  }
  EXPECT_EQ(blk->current_bin, 0);
}


TEST_F(ArenaBinTest, ExactBoundaryTransitions) {
  // Verify each quarter boundary triggers the right bin transition.
  // We do this with four objects, each crossing one boundary in sequence.
  ArenaBlock *blk = heap.newBlock();
  ASSERT_NE(blk, nullptr);

  size_t quarter = arena_size / 4;

  ObjectHeader *o1 = alloc_real(blk, quarter);
  ASSERT_NE(o1, nullptr);
  blk->free(o1);
  EXPECT_EQ(blk->current_bin, 1);

  ObjectHeader *o2 = alloc_real(blk, quarter);
  ASSERT_NE(o2, nullptr);
  blk->free(o2);
  EXPECT_EQ(blk->current_bin, 2);

  ObjectHeader *o3 = alloc_real(blk, quarter);
  ASSERT_NE(o3, nullptr);
  blk->free(o3);
  EXPECT_EQ(blk->current_bin, 3);

  ObjectHeader *o4 = alloc_real(blk, quarter);
  ASSERT_NE(o4, nullptr);
  blk->free(o4);
  EXPECT_EQ(blk->current_bin, 4);
}