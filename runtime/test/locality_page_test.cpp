#include <gtest/gtest.h>
#include <alaska.h>
#include "alaska/util/Logger.hpp"
#include "gtest/gtest.h"
#include <vector>
#include <alaska/heaps/Heap.hpp>

#include <alaska/core/Runtime.hpp>
#include <alaska/util/handle_ptr.hpp>

#define DUMMY_THREADCACHE ((alaska::ThreadCache *)0x1000UL)


#if 0
// A locality slab does not have to live within a LocalityPage, so we can
// test it independently of the LocalityPage.
class LocalitySlabTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Make it so we only get warnings
    alaska::set_log_level(LOG_WARN);
    tc = runtime.new_threadcache();
    // construct the slab
    slab->init();
  }

  void *checked_alloc(size_t size, alaska::Mapping *mapping) {
    void *ptr = slab->alloc(size, *mapping);
    if (ptr != nullptr) {
      // validate that the allocation's bounds are within slab_memory
      EXPECT_GE(ptr, slab_memory);
      EXPECT_LE((uintptr_t)ptr + size, (uintptr_t)slab_memory + alaska::locality_slab_size);
    }

    return ptr;
  }

  void TearDown() override { runtime.del_threadcache(tc); }


  alaska::ThreadCache *tc;
  alaska::Runtime runtime;
  uint8_t slab_memory[alaska::locality_slab_size];
  alaska::LocalitySlab *slab = (alaska::LocalitySlab *)slab_memory;
};

TEST_F(LocalitySlabTest, Sanity) {
  // This test is just to make sure that the runtime is correctly initialized.
  EXPECT_EQ(slab->bump_size, 0);
  EXPECT_EQ(slab->freed, 0);
}

TEST_F(LocalitySlabTest, AllocationBumpsCorrectly) {
  // This test is just to make sure that the runtime is correctly initialized.
  EXPECT_EQ(slab->bump_size, 0);
  EXPECT_EQ(slab->freed, 0);
  auto avail_start = slab->available();

  auto *mapping = tc->new_mapping();
  void *ptr = checked_alloc(8, mapping);
  EXPECT_EQ(slab->freed, 0);
  EXPECT_EQ(slab->bump_size, 8 + sizeof(alaska::ObjectHeader));

  // availabile should reduce
  EXPECT_LT(slab->available(), avail_start);
  EXPECT_EQ(slab->get_size(ptr), 8);
}


TEST_F(LocalitySlabTest, FreeingTracksCorrectly) {
  // This test is just to make sure that the runtime is correctly initialized.
  EXPECT_EQ(slab->bump_size, 0);
  EXPECT_EQ(slab->freed, 0);

  auto *mapping = tc->new_mapping();
  void *ptr = checked_alloc(8, mapping);
  slab->free(ptr);
  EXPECT_EQ(slab->freed, 8 + sizeof(alaska::ObjectHeader));
}


// Write a test that allocates an object that is too big and asserts it returns 0
TEST_F(LocalitySlabTest, AllocationTooBig) {
  // This test is just to make sure that the runtime is correctly initialized.
  EXPECT_EQ(slab->bump_size, 0);
  EXPECT_EQ(slab->freed, 0);

  auto *mapping = tc->new_mapping();
  void *ptr = checked_alloc(alaska::locality_slab_size * 2, mapping);
  EXPECT_EQ(ptr, nullptr);
  EXPECT_EQ(slab->freed, 0);
  EXPECT_EQ(slab->bump_size, 0);
}



class LocalityPageTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Make it so we only get warnings
    alaska::set_log_level(LOG_WARN);
    tc = runtime.new_threadcache();
  }
  void TearDown() override { runtime.del_threadcache(tc); }

  alaska::ThreadCache *tc;
  alaska::Runtime runtime;
};




TEST_F(LocalityPageTest, Sanity) {
  // This test is just to make sure that the runtime is correctly initialized.
}


// TEST_F(LocalityPageTest, LocalizeInvalid) {
//   // It should be invalid to attempt to localize a non-handle
//   int value = 0;
//   ASSERT_FALSE(tc->localize((void *)&value, runtime.localization_epoch));
// }


// TEST_F(LocalityPageTest, LocalizeAllocation) {
//   // Allocate a handle, then localize it.
//   alaska::sim::handle_ptr<int> h = (int *)tc->halloc(32);
//   ASSERT_NE(h.get(), nullptr);

//   void *orig_location = h.translate();
//   ASSERT_NE(orig_location, nullptr);

//   *h = 42;
//   ASSERT_EQ(*h, 42);

//   bool loc = tc->localize(h.get(), runtime.localization_epoch);
//   ASSERT_TRUE(loc);

//   void *new_location = h.translate();
//   ASSERT_NE(new_location, nullptr);
//   ASSERT_NE(new_location, orig_location);

//   ASSERT_EQ(*h, 42);
// }
#endif
