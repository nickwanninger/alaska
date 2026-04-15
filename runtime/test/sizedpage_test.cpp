#include <gtest/gtest.h>
#include <alaska.h>
#include "alaska/util/Logger.hpp"
#include "gtest/gtest.h"
#include <vector>
#include <algorithm>
#include <alaska/heaps/Heap.hpp>
#include <alaska/core/Runtime.hpp>
#include <alaska/heaps/SizedPage.hpp>

class SizedPageTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Make it so we only get warnings
    alaska::set_log_level(LOG_WARN);
    sp = rt.heap.get_sizedpage(16, nullptr /* Dubious if passing null here is okay. */);
    hs = rt.handle_table.fresh_slab();
  }
  void TearDown() override {}

  alaska::Mapping *alloc() {
    alaska::Mapping *m = hs->alloc();
    EXPECT_NE(m, nullptr);

    void *d = sp->alloc(*m, 16);
    if (d == NULL) {
      hs->free(m);
      return nullptr;
    }

    m->set_pointer(d);
    return m;
  }
  void release(alaska::Mapping *m) {
    sp->release_local(*m, m->get_pointer());
    hs->free(m);
  }

  alaska::Runtime rt;
  alaska::HandleSlab *hs;
  alaska::SizedPage *sp;
};


TEST_F(SizedPageTest, Sanity) { EXPECT_NE(sp, nullptr); }
TEST_F(SizedPageTest, AllocationWorks) {
  // Make sure we can allocate a mapping
  EXPECT_NE(alloc(), nullptr);
}

TEST_F(SizedPageTest, AllocationIsUnique) {
  // Make sure we can allocate a mapping
  alaska::Mapping *m1 = alloc();
  alaska::Mapping *m2 = alloc();

  EXPECT_NE(m1, m2);
}


TEST_F(SizedPageTest, Reuse) {
  alaska::Mapping *m1 = alloc();
  release(m1);
  alaska::Mapping *m2 = alloc();
  EXPECT_EQ(m1, m2);
}

TEST_F(SizedPageTest, ObjectsIterationWithVariedSizes) {
  // This test validates that iteration via page->objects() works correctly
  // even when objects are allocated with sizes that don't perfectly match
  // the SizedPage's size class (they get rounded up by AlignedSize).
  
  // Create a new page for this test
  alaska::SizedPage *page = rt.heap.get_sizedpage(32, nullptr);
  EXPECT_NE(page, nullptr);
  
  alaska::HandleSlab *test_hs = rt.handle_table.fresh_slab();
  
  // Allocate several objects with varied sizes
  // All will be rounded to the size class internally
  std::vector<alaska::Mapping *> allocated_mappings;
  std::vector<void *> allocated_ptrs;
  
  size_t sizes_to_try[] = {10, 25, 32, 30, 16};
  
  for (size_t size : sizes_to_try) {
    auto m = test_hs->alloc();
    EXPECT_NE(m, nullptr);
    
    void *ptr = page->alloc(*m, alaska::AlignedSize(size));
    EXPECT_NE(ptr, nullptr) << "Failed to allocate size " << size;
    
    m->set_pointer(ptr);
    allocated_mappings.push_back(m);
    allocated_ptrs.push_back(ptr);
  }
  
  // Now iterate using objects() and verify all allocated objects are found
  std::vector<uint32_t> found_handle_ids;
  size_t object_count = 0;
  
  for (auto *obj : page->objects()) {
    EXPECT_NE(obj, nullptr);
    // Count all objects iterated
    object_count++;
    
    // Only track objects that are actually allocated (non-zero handle_id)
    if (obj->handle_id != 0) {
      found_handle_ids.push_back(obj->handle_id);
    }
  }
  
  // Verify that we found all the allocated objects
  EXPECT_GE(object_count, allocated_mappings.size()) 
    << "Should find at least as many objects as we allocated";
  
  // Verify that the number of non-zero handle IDs matches our allocations
  EXPECT_EQ(found_handle_ids.size(), allocated_mappings.size())
    << "Should find exactly as many allocated objects as we created";
  
  // Verify that all allocated handle IDs are in the found list
  for (size_t i = 0; i < allocated_mappings.size(); i++) {
    uint32_t expected_id = allocated_mappings[i]->handle_id();
    auto it = std::find(found_handle_ids.begin(), found_handle_ids.end(), expected_id);
    EXPECT_NE(it, found_handle_ids.end()) 
      << "Should find allocated object with handle_id " << expected_id;
  }
  
  // Clean up
  for (auto m : allocated_mappings) {
    page->release_local(*m, m->get_pointer());
    test_hs->free(m);
  }
}

TEST_F(SizedPageTest, ObjectsIterationOrder) {
  // This test validates that objects are iterated in memory order
  // (the order they appear in the page), regardless of their requested sizes.
  
  alaska::SizedPage *page = rt.heap.get_sizedpage(16, nullptr);
  EXPECT_NE(page, nullptr);
  
  alaska::HandleSlab *test_hs = rt.handle_table.fresh_slab();
  
  // Allocate a small number of objects
  std::vector<alaska::Mapping *> mappings;
  for (int i = 0; i < 3; i++) {
    auto m = test_hs->alloc();
    EXPECT_NE(m, nullptr);
    
    void *ptr = page->alloc(*m, alaska::AlignedSize(16));
    EXPECT_NE(ptr, nullptr);
    
    m->set_pointer(ptr);
    mappings.push_back(m);
  }
  
  // Collect the object headers in iteration order
  std::vector<alaska::ObjectHeader *> iterated_objects;
  for (auto *obj : page->objects()) {
    if (obj->handle_id != 0) {
      iterated_objects.push_back(obj);
    }
  }
  
  // Verify we found all objects
  EXPECT_EQ(iterated_objects.size(), mappings.size());
  
  // Verify they are in memory order (each successive object should be
  // further in memory than the previous one)
  for (size_t i = 1; i < iterated_objects.size(); i++) {
    uintptr_t prev_addr = (uintptr_t)iterated_objects[i - 1];
    uintptr_t curr_addr = (uintptr_t)iterated_objects[i];
    EXPECT_LT(prev_addr, curr_addr) 
      << "Objects should be iterated in ascending memory order";
  }
  
  // Clean up
  for (auto m : mappings) {
    page->release_local(*m, m->get_pointer());
    test_hs->free(m);
  }
}

TEST_F(SizedPageTest, ObjectsIterationHandlesFreedObjects) {
  // This test validates that iteration correctly handles pages with
  // both allocated and freed objects.
  
  alaska::SizedPage *page = rt.heap.get_sizedpage(16, nullptr);
  EXPECT_NE(page, nullptr);
  
  alaska::HandleSlab *test_hs = rt.handle_table.fresh_slab();
  
  // Allocate several objects
  std::vector<alaska::Mapping *> mappings;
  for (int i = 0; i < 5; i++) {
    auto m = test_hs->alloc();
    EXPECT_NE(m, nullptr);
    
    void *ptr = page->alloc(*m, alaska::AlignedSize(16));
    EXPECT_NE(ptr, nullptr);
    
    m->set_pointer(ptr);
    mappings.push_back(m);
  }
  
  // Free some objects (at indices 1 and 3)
  page->release_local(*mappings[1], mappings[1]->get_pointer());
  page->release_local(*mappings[3], mappings[3]->get_pointer());
  
  // Count allocated vs freed objects
  size_t allocated_count = 0;
  size_t freed_count = 0;
  
  for (auto *obj : page->objects()) {
    if (obj->handle_id != 0) {
      allocated_count++;
    } else {
      // Check if this was one of our freed objects (by checking if it was in our list)
      // A freed object will have handle_id == 0
      freed_count++;
    }
  }
  
  // We should have found some freed objects (those are typically reused)
  // The exact count depends on the internal state, but we should find
  // at least 3 allocated objects (indices 0, 2, 4)
  EXPECT_GE(allocated_count, 3) 
    << "Should find at least the non-freed allocated objects";
  
  // Clean up remaining objects
  for (int i = 0; i < 5; i++) {
    if (i != 1 && i != 3) {  // Skip already-freed objects
      page->release_local(*mappings[i], mappings[i]->get_pointer());
    }
    test_hs->free(mappings[i]);
  }
}
