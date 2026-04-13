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

TEST_F(ArenaBinTest, CompactNoopWhenNoFreedBytes) {
  Runtime rt;
  ThreadCache *tc = rt.new_threadcache();

  void *h1 = tc->halloc(16);
  void *h2 = tc->halloc(24);
  ASSERT_NE(h1, nullptr);
  ASSERT_NE(h2, nullptr);

  Mapping *m1 = Mapping::from_handle_safe(h1);
  Mapping *m2 = Mapping::from_handle_safe(h2);
  ASSERT_NE(m1, nullptr);
  ASSERT_NE(m2, nullptr);

  ArenaBlock *blk = get_arena_block(m1->get_pointer());
  ASSERT_EQ(get_arena_block(m2->get_pointer()), blk);

  void *p1_before = m1->get_pointer();
  void *p2_before = m2->get_pointer();
  size_t available_before = blk->available();

  size_t reclaimed = blk->compact();
  EXPECT_EQ(reclaimed, 0u);
  EXPECT_EQ(m1->get_pointer(), p1_before);
  EXPECT_EQ(m2->get_pointer(), p2_before);
  EXPECT_EQ(blk->available(), available_before);

  tc->hfree(h1);
  tc->hfree(h2);
  rt.del_threadcache(tc);
}


TEST_F(ArenaBinTest, CompactPacksLiveObjectsAndUpdatesMappings) {
  Runtime rt;
  ThreadCache *tc = rt.new_threadcache();

  void *h1 = tc->halloc(16);
  void *hdead = tc->halloc(24);
  void *h2 = tc->halloc(32);
  ASSERT_NE(h1, nullptr);
  ASSERT_NE(hdead, nullptr);
  ASSERT_NE(h2, nullptr);

  Mapping *m1 = Mapping::from_handle_safe(h1);
  Mapping *mdead = Mapping::from_handle_safe(hdead);
  Mapping *m2 = Mapping::from_handle_safe(h2);
  ASSERT_NE(m1, nullptr);
  ASSERT_NE(mdead, nullptr);
  ASSERT_NE(m2, nullptr);

  ObjectHeader *live1 = ObjectHeader::from(m1->get_pointer());
  ObjectHeader *dead = ObjectHeader::from(mdead->get_pointer());
  ObjectHeader *live2 = ObjectHeader::from(m2->get_pointer());

  ArenaBlock *blk = get_arena_block(m1->get_pointer());
  ASSERT_EQ(get_arena_block(mdead->get_pointer()), blk);
  ASSERT_EQ(get_arena_block(m2->get_pointer()), blk);

  *reinterpret_cast<uint64_t *>(live1->data()) = 0x1111222233334444ULL;
  *reinterpret_cast<uint64_t *>(live2->data()) = 0x5555666677778888ULL;

  void *live2_before = m2->get_pointer();
  size_t available_before = blk->available();
  size_t dead_size = dead->real_object_size();

  tc->hfree(hdead);
  ASSERT_GT(blk->freed_bytes, 0u);

  size_t reclaimed = blk->compact();

  EXPECT_EQ(reclaimed, dead_size);
  EXPECT_EQ(blk->available(), available_before + reclaimed);
  EXPECT_EQ(blk->freed_bytes, 0u);

  auto *live1_after = ObjectHeader::from(m1->get_pointer());
  auto *live2_after = ObjectHeader::from(m2->get_pointer());
  EXPECT_EQ(*reinterpret_cast<uint64_t *>(live1_after->data()), 0x1111222233334444ULL);
  EXPECT_EQ(*reinterpret_cast<uint64_t *>(live2_after->data()), 0x5555666677778888ULL);
  EXPECT_EQ((char *)m2->get_pointer(), (char *)live2_before - dead_size);

  tc->hfree(h1);
  tc->hfree(h2);
  rt.del_threadcache(tc);
}


TEST_F(ArenaBinTest, CompactFullyDeadBlockTransitionsToReadyBin) {
  Runtime rt;
  ThreadCache *tc = rt.new_threadcache();

  void *h = tc->halloc(40);
  ASSERT_NE(h, nullptr);

  Mapping *m = Mapping::from_handle_safe(h);
  ASSERT_NE(m, nullptr);

  ObjectHeader *obj = ObjectHeader::from(m->get_pointer());
  ArenaBlock *blk = get_arena_block(m->get_pointer());

  tc->hfree(h);

  size_t reclaimed = blk->compact();
  EXPECT_EQ(reclaimed, obj->real_object_size());
  EXPECT_EQ(blk->available(), arena_size);
  EXPECT_EQ(blk->freed_bytes, arena_size);
  EXPECT_EQ(blk->current_bin, 4);

  ArenaBlock *reused = rt.arena_heap.checkout_block();
  EXPECT_EQ(reused, blk);
  EXPECT_EQ(reused->current_bin, 0);
  EXPECT_EQ(reused->freed_bytes, 0u);

  rt.del_threadcache(tc);
}
