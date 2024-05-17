#include <gtest/gtest.h>
#include <alaska.h>
#include <alaska/table.hpp>
#include "alaska/Logger.hpp"
#include "gtest/gtest.h"
#include <vector>

#include <alaska/Runtime.hpp>

// The tests in this file are all based around testing invariants about the runtime of Alaska.

class RuntimeTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Make it so we only get warnings
    alaska::set_log_level(LOG_WARN);
  }
  void TearDown() override {}

  alaska::Runtime runtime;
};




TEST_F(RuntimeTest, Sanity) {
  // This test is just to make sure that the runtime is correctly initialized.
}



TEST_F(RuntimeTest, TableInitializedCorrectly) {
  // The runtime should have a handle table with no slabs active.
  ASSERT_EQ(runtime.handle_table.slab_count(), 0);
  // And the capacity should be the initial capacity.
  ASSERT_EQ(runtime.handle_table.capacity(), alaska::HandleTable::initial_capacity);
}



TEST_F(RuntimeTest, OnlyOneRuntime) {
  ASSERT_DEATH(
      {
        // Simply create a second runtime. This will cause an abort.
        alaska::Runtime runtime2;
      },
      "");
}




TEST_F(RuntimeTest, FreshSlabAllocation) {
  // Allocate a fresh slab from the handle table
  auto* slab = runtime.handle_table.fresh_slab();
  // Check that the allocated slab is not null
  ASSERT_NE(slab, nullptr);
}




TEST_F(RuntimeTest, FreshSlabAllocationIncreasesSlabCount) {
  // Get the initial slab count
  int initialSlabCount = runtime.handle_table.slab_count();

  // Allocate a fresh slab from the handle table
  auto* slab = runtime.handle_table.fresh_slab();

  // Check that the allocated slab is not null
  ASSERT_NE(slab, nullptr);

  // Check that the slab count has increased by 1
  ASSERT_EQ(runtime.handle_table.slab_count(), initialSlabCount + 1);
}




TEST_F(RuntimeTest, UniqueSlabAllocations) {
  // Allocate multiple fresh slabs from the handle table
  auto* slab1 = runtime.handle_table.fresh_slab();
  auto* slab2 = runtime.handle_table.fresh_slab();
  auto* slab3 = runtime.handle_table.fresh_slab();

  // Check that all allocated slabs are not null
  ASSERT_NE(slab1, nullptr);
  ASSERT_NE(slab2, nullptr);
  ASSERT_NE(slab3, nullptr);

  // Check that all allocated slabs are unique
  ASSERT_NE(slab1, slab2);
  ASSERT_NE(slab1, slab3);
  ASSERT_NE(slab2, slab3);
}




TEST_F(RuntimeTest, CapacityGrowth) {
  // Allocate a large number of slabs
  for (int i = 0; i < alaska::HandleTable::initial_capacity * 2; i++) {
    auto* slab = runtime.handle_table.fresh_slab();
    ASSERT_NE(slab, nullptr);
  }

  // Check that the capacity of the handle table has grown
  ASSERT_GT(runtime.handle_table.capacity(), alaska::HandleTable::initial_capacity);
}



TEST_F(RuntimeTest, SlabGetHandle) {
  // Allocate a fresh slab from the handle table
  auto* slab = runtime.handle_table.fresh_slab();

  // Get a handle from the slab
  auto handle = slab->get();

  // Check that the handle is valid
  ASSERT_NE(handle, nullptr);
}

TEST_F(RuntimeTest, SlabNFreeDecreasesOnHandleGet) {
  // Allocate a fresh slab from the handle table
  auto* slab = runtime.handle_table.fresh_slab();
  // Get the initial nfree count
  int initialNFree = slab->nfree;
  // Get a handle from the slab
  auto handle = slab->get();
  // Check that the handle is valid
  ASSERT_NE(handle, nullptr);
  // Check that the nfree count has decreased by 1
  ASSERT_EQ(slab->nfree, initialNFree - 1);
}

TEST_F(RuntimeTest, SlabNFreeOnInitialAllocation) {
  // Allocate a fresh slab from the handle table
  auto* slab = runtime.handle_table.fresh_slab();
  // Get the initial nfree count
  int initialNFree = slab->nfree;
  // Check that the nfree count is equal to the slab's capacity
  ASSERT_EQ(initialNFree, alaska::HandleTable::slab_capacity);
}



// Make sure that for many slabs, get_slab returns the right one
TEST_F(RuntimeTest, SlabGetSlab) {
  // Allocate a large number of slabs
  for (int i = 0; i < alaska::HandleTable::initial_capacity * 2; i++) {
    auto* slab = runtime.handle_table.fresh_slab();
    ASSERT_NE(slab, nullptr);
    // Check that get_slab returns the right slab
    ASSERT_EQ(runtime.handle_table.get_slab(slab->idx), slab);
  }
}


TEST_F(RuntimeTest, SlabGetReturnsNullWhenOutOfCapacity) {
  // Allocate a fresh slab from the handle table
  auto* slab = runtime.handle_table.fresh_slab();
  // Fill up the slab with handles
  for (int i = 0; i < alaska::HandleTable::slab_capacity; i++) {
    auto handle = slab->get();
    ASSERT_NE(handle, nullptr);
  }
  // Try to get another handle from the slab
  auto handle = slab->get();
  // Check that the handle is null
  ASSERT_EQ(handle, nullptr);
}


// Test that a slab returns all unique handles
TEST_F(RuntimeTest, SlabUniqueHandles) {
  // Allocate a fresh slab from the handle table
  auto* slab = runtime.handle_table.fresh_slab();
  // Create a set to store the handles
  std::set<alaska::Mapping*> handles;
  // Fill up the slab with handles
  for (int i = 0; i < alaska::HandleTable::slab_capacity; i++) {
    auto handle = slab->get();
    ASSERT_NE(handle, nullptr);
    // Check that the handle is unique
    ASSERT_EQ(handles.count(handle), 0);
    handles.insert(handle);
  }
}



// test that a slab returns the same mapping once it is put back
TEST_F(RuntimeTest, SlabReturnHandle) {
  // Allocate a fresh slab from the handle table
  auto* slab = runtime.handle_table.fresh_slab();
  // Get a handle from the slab
  auto handle = slab->get();
  // Return the handle to the slab
  slab->put(handle);
  // Get another handle from the slab
  auto handle2 = slab->get();
  // Check that the handle is the same as the one that was returned the first time
  ASSERT_EQ(handle, handle2);
}



// test that the program dies if a mapping is returned to the wrong slab
TEST_F(RuntimeTest, SlabReturnToWrongSlab) {
  // Allocate a fresh slab from the handle table
  auto* slab1 = runtime.handle_table.fresh_slab();
  auto* slab2 = runtime.handle_table.fresh_slab();
  // Get a handle from slab1
  auto handle = slab1->get();
  // Try to return the handle to slab2
  ASSERT_DEATH({ slab2->put(handle); }, "");
}


// test that all mappings in a slab are in the right index
TEST_F(RuntimeTest, SlabMappingIndex) {
  // For a few iterations...
  for (int slabi = 0; slabi < 10; slabi++) {
    // Allocate a fresh slab from the handle table
    auto* slab = runtime.handle_table.fresh_slab();
    // Fill up the slab with handles
    for (int i = 0; i < alaska::HandleTable::slab_capacity; i++) {
      auto handle = slab->get();
      ASSERT_NE(handle, nullptr);
      // Check that the handle is in the right index
      ASSERT_EQ(slab->idx, runtime.handle_table.mapping_slab_idx(handle));
    }
  }
}


//////////////////////
// Handle Slab Queue
//////////////////////

// Test that HandleSlabQueues pop all the slabs that are pushed
TEST_F(RuntimeTest, HandleSlabQueuePop) {
  // Create a queue
  alaska::HandleSlabQueue queue;
  ASSERT_TRUE(queue.empty());

  // Create a vector to store the slabs
  std::vector<alaska::HandleSlab*> slabs;
  // For a few iterations...
  for (int i = 0; i < 10; i++) {
    // Allocate a fresh slab from the handle table
    auto* slab = runtime.handle_table.fresh_slab();
    // Push the slab to the queue
    queue.push(slab);
    // Store the slab in the vector
    slabs.push_back(slab);
  }

  // Pop all the slabs from the queue
  for (int i = 0; i < 10; i++) {
    auto* slab = queue.pop();
    // Check that the slab is the same as the one that was pushed
    ASSERT_EQ(slab, slabs[i]);
  }
  ASSERT_TRUE(queue.empty());
}


TEST_F(RuntimeTest, HandleSlabQueuePopEmpty) {
  alaska::HandleSlabQueue queue;
  ASSERT_TRUE(queue.empty());
  ASSERT_EQ(nullptr, queue.pop());
}

// Test that removing a slab from the queue makes it empty
TEST_F(RuntimeTest, HandleSlabQueueEmpty) {
  alaska::HandleSlabQueue queue;
  ASSERT_TRUE(queue.empty());
  auto* slab = runtime.handle_table.fresh_slab();
  queue.push(slab);
  ASSERT_FALSE(queue.empty());
  queue.pop();
  ASSERT_TRUE(queue.empty());
}

// Test that removing a slab from the start of the queue works.
TEST_F(RuntimeTest, HandleSlabQueueRemoveStart) {
  alaska::HandleSlabQueue queue;
  auto* slab1 = runtime.handle_table.fresh_slab();
  auto* slab2 = runtime.handle_table.fresh_slab();
  auto* slab3 = runtime.handle_table.fresh_slab();

  queue.push(slab1);
  queue.push(slab2);
  queue.push(slab3);

  queue.remove(slab1);
  ASSERT_EQ(queue.pop(), slab2);
  ASSERT_EQ(queue.pop(), slab3);
}

// Test that removing a slab from the middle of the queue works.
TEST_F(RuntimeTest, HandleSlabQueueRemoveMiddle) {
  alaska::HandleSlabQueue queue;
  auto* slab1 = runtime.handle_table.fresh_slab();
  auto* slab2 = runtime.handle_table.fresh_slab();
  auto* slab3 = runtime.handle_table.fresh_slab();

  queue.push(slab1);
  queue.push(slab2);
  queue.push(slab3);

  queue.remove(slab2);
  ASSERT_EQ(queue.pop(), slab1);
  ASSERT_EQ(queue.pop(), slab3);
}

// Test that removing a slab from the end of the queue works.
TEST_F(RuntimeTest, HandleSlabQueueRemoveEnd) {
  alaska::HandleSlabQueue queue;
  auto* slab1 = runtime.handle_table.fresh_slab();
  auto* slab2 = runtime.handle_table.fresh_slab();
  auto* slab3 = runtime.handle_table.fresh_slab();

  queue.push(slab1);
  queue.push(slab2);
  queue.push(slab3);

  queue.remove(slab3);
  ASSERT_EQ(queue.pop(), slab1);
  ASSERT_EQ(queue.pop(), slab2);
}