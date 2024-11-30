// This file tests alaska::HugeObjectAllocator

#include <alaska/ObjectAllocator.hpp>
#include "alaska/utils.h"
#include <gtest/gtest.h>
#include "alaska/Logger.hpp"



class ObjectAllocatorTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Make it so we only get warnings
    alaska::set_log_level(LOG_WARN);



    buffer = calloc(object_count, object_size);
    salloc.configure(buffer, object_count);
  }
  void TearDown() override { free(buffer); }


  long object_size = 16;
  long object_count = 512;
  void *buffer;
  alaska::ObjectAllocator<uint64_t> salloc;
};




TEST_F(ObjectAllocatorTest, Sanity) {
  // Out of the gate, the number of free objects must be equal to the number of objects
  ASSERT_EQ(salloc.num_free(), object_count);
}

TEST_F(ObjectAllocatorTest, Extend) {
  ASSERT_EQ(salloc.num_free_in_bump_allocator(), object_count);
  long count = salloc.extend(1);

  ASSERT_EQ(salloc.num_free_in_free_list(), 1);
  // out of the gate, the number of free objects must be equal to the number of objects
  ASSERT_EQ(salloc.num_free_in_bump_allocator(), object_count - count);
}


TEST_F(ObjectAllocatorTest, Allocate) {
  auto *b = salloc.alloc();
  ASSERT_NE(b, nullptr);
}


TEST_F(ObjectAllocatorTest, AllocateNumFree) {
  auto initial_nfree = salloc.num_free();
  salloc.alloc();
  ASSERT_EQ(salloc.num_free(), initial_nfree - 1);
}


TEST_F(ObjectAllocatorTest, ReleaseLocalNumFree) {
  auto *b = salloc.alloc();
  ASSERT_NE(b, nullptr);
  ASSERT_EQ(salloc.num_free(), object_count - 1);

  salloc.release_local(b);
  ASSERT_EQ(salloc.num_free(), object_count);
}

TEST_F(ObjectAllocatorTest, ReleaseRemoteNumFree) {
  auto *b = salloc.alloc();
  ASSERT_NE(b, nullptr);
  ASSERT_EQ(salloc.num_free(), object_count - 1);

  salloc.release_local(b);
  ASSERT_EQ(salloc.num_free(), object_count);
}
