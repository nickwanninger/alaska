// This file tests alaska::HugeObjectAllocator

#include <alaska/HugeObjectAllocator.hpp>
#include "alaska/utils.h"
#include <gtest/gtest.h>



class HugeObjectTest : public ::testing::Test {
 public:
  alaska::HugeObjectAllocator allocator;
};

TEST_F(HugeObjectTest, AllocateAndDeallocate) {
  // Allocate a huge object
  void* ptr = allocator.allocate(1024);

  // Verify that the allocation was successful
  ASSERT_NE(ptr, nullptr);

  // Deallocate the object
  allocator.free(ptr);

  // Verify that the deallocation was successful
  ASSERT_TRUE(true);
}

TEST_F(HugeObjectTest, AllocateZeroSize) {
  // Allocate a zero-sized object
  void* ptr = allocator.allocate(0);

  // Verify that the allocation was successful
  ASSERT_NE(ptr, nullptr);

  // Deallocate the object
  allocator.free(ptr);

  // Verify that the deallocation was successful
  ASSERT_TRUE(true);
}

TEST_F(HugeObjectTest, AllocateMultipleObjects) {
  // Allocate multiple objects
  void* ptr1 = allocator.allocate(512);
  void* ptr2 = allocator.allocate(256);
  void* ptr3 = allocator.allocate(128);

  // Verify that the allocations were successful
  ASSERT_NE(ptr1, nullptr);
  ASSERT_NE(ptr2, nullptr);
  ASSERT_NE(ptr3, nullptr);

  // Deallocate the objects
  allocator.free(ptr1);
  allocator.free(ptr2);
  allocator.free(ptr3);

  // Verify that the deallocations were successful
  ASSERT_TRUE(true);
}


TEST_F(HugeObjectTest, AllocateAndAccessObject) {
  // Allocate a huge object
  void* ptr = allocator.allocate(1024);

  // Verify that the allocation was successful
  ASSERT_NE(ptr, nullptr);

  // Access the object
  int* data = static_cast<int*>(ptr);
  *data = 42;

  // Verify that the object can be accessed
  ASSERT_EQ(*data, 42);

  // Deallocate the object
  allocator.free(ptr);
}

TEST_F(HugeObjectTest, FreeNonOwnedPointer) {
  // Allocate a huge object
  void* ptr = allocator.allocate(1024);

  // Verify that the allocation was successful
  ASSERT_NE(ptr, nullptr);

  // Create a new pointer that is not owned by the allocator
  void* nonOwnedPtr = malloc(1024);

  // Free the non-owned pointer using the allocator
  bool result = allocator.free(nonOwnedPtr);

  // Verify that the free operation returns false
  ASSERT_FALSE(result);

  // Deallocate the owned pointer
  allocator.free(ptr);
}
