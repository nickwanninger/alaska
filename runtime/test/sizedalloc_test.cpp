#include <gtest/gtest.h>
#include <alaska.h>
#include "alaska/Logger.hpp"
#include "gtest/gtest.h"
#include <vector>
#include <alaska/Heap.hpp>

#include <alaska/SizedAllocator.hpp>



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
  // Out of the gate, the number of free objects must be equal to the number of objects
  ASSERT_EQ(salloc.num_free(), object_count);
}

TEST_F(SizedAllocatorTest, Extend) {
  ASSERT_EQ(salloc.num_free_in_bump_allocator(), object_count);
  long count = salloc.extend(1);

  ASSERT_EQ(salloc.num_free_in_free_list(), 1);
  // out of the gate, the number of free objects must be equal to the number of objects
  ASSERT_EQ(salloc.num_free_in_bump_allocator(), object_count - count);
}


TEST_F(SizedAllocatorTest, Allocate) {
  void *b = salloc.alloc();
  ASSERT_NE(b, nullptr);
}


TEST_F(SizedAllocatorTest, AllocateNumFree) {
  auto initial_nfree = salloc.num_free();
  salloc.alloc();
  ASSERT_EQ(salloc.num_free(), initial_nfree - 1);
}


TEST_F(SizedAllocatorTest, ReleaseLocalNumFree) {
  void *b = salloc.alloc();
  ASSERT_NE(b, nullptr);
  ASSERT_EQ(salloc.num_free(), object_count - 1);

  salloc.release_local(b);
  ASSERT_EQ(salloc.num_free(), object_count);
}

TEST_F(SizedAllocatorTest, ReleaseRemoteNumFree) {
  void *b = salloc.alloc();
  ASSERT_NE(b, nullptr);
  ASSERT_EQ(salloc.num_free(), object_count - 1);

  salloc.release_local(b);
  ASSERT_EQ(salloc.num_free(), object_count);
}
