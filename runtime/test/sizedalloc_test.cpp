#include <gtest/gtest.h>
#include <alaska.h>
#include "alaska/util/Logger.hpp"
#include "gtest/gtest.h"
#include <vector>
#include <alaska/heaps/Heap.hpp>

#include <alaska/util/SizedAllocator.hpp>



class SizedAllocatorTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Make it so we only get warnings
    alaska::set_log_level(LOG_WARN);



    buffer = calloc(object_count, object_size);
    salloc.configure(buffer, object_size, object_count);
  }
  void TearDown() override { free(buffer); }


  long object_size = 16;
  long object_count = 512;
  void *buffer;
  alaska::SizedAllocator salloc;
};




TEST_F(SizedAllocatorTest, Sanity) {
  // Out of the gate, the bump allocator should have all objects available
  ASSERT_EQ(salloc.num_free_in_bump_allocator(), object_count);
}

TEST_F(SizedAllocatorTest, Extend) {
  ASSERT_EQ(salloc.num_free_in_bump_allocator(), object_count);
  long count = salloc.extend(1);

  ASSERT_TRUE(salloc.some_available());
  ASSERT_EQ(salloc.num_free_in_bump_allocator(), object_count - count);
}


TEST_F(SizedAllocatorTest, Allocate) {
  void *b = salloc.alloc();
  ASSERT_NE(b, nullptr);
}


TEST_F(SizedAllocatorTest, AllocateReducesAvailability) {
  ASSERT_TRUE(salloc.some_available());
  salloc.alloc();
  ASSERT_TRUE(salloc.some_available());
}


TEST_F(SizedAllocatorTest, ReleaseLocalRestoresAvailability) {
  void *b = salloc.alloc();
  ASSERT_NE(b, nullptr);
  ASSERT_TRUE(salloc.some_available());

  salloc.release_local(b);
  ASSERT_TRUE(salloc.some_available());
}

TEST_F(SizedAllocatorTest, ReleaseRemoteRestoresAvailability) {
  void *b = salloc.alloc();
  ASSERT_NE(b, nullptr);
  ASSERT_TRUE(salloc.some_available());

  salloc.release_local(b);
  ASSERT_TRUE(salloc.some_available());
}
