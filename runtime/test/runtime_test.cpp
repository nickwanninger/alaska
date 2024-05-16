#include <gtest/gtest.h>
#include <alaska.h>
#include <alaska/table.hpp>
#include "gtest/gtest.h"

#include <alaska/Runtime.hpp>

// The tests in this file are all based around testing invariants about the runtime of Alaska.

class RuntimeTest : public ::testing::Test {
 public:
  void SetUp() override {}
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



// TEST_F(RuntimeTest, HallocWorks) {
//   // Allocate a handle
//   auto handle = runtime.halloc(16);
//   // Check that the handle is valid
//   ASSERT_NE(handle, nullptr);
// }