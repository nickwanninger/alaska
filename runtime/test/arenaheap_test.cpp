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