#include <gtest/gtest.h>
#include <alaska.h>
#include "alaska/Logger.hpp"
#include "gtest/gtest.h"
#include <vector>
#include <alaska/Heap.hpp>

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




TEST_F(RuntimeTest, HandleTableGetMapping) {
  // Get a mapping from the handle table
  auto m = runtime.handle_table.get();
  // Check that the mapping is not null
  ASSERT_NE(m, nullptr);
  runtime.handle_table.put(m);
}


TEST_F(RuntimeTest, HandleTableGetUniqueValues) {
  // Create a set to store the mappings
  std::set<alaska::Mapping*> mappings;
  // Get multiple mappings from the handle table
  for (int i = 0; i < 1000; i++) {
    auto m = runtime.handle_table.get();
    // Check that the mapping is not null
    ASSERT_NE(m, nullptr);
    // Check that the mapping is unique
    ASSERT_EQ(mappings.count(m), 0);
    mappings.insert(m);
  }
}

////////////////////////////////////////////////////////////////////////


class PageManagerTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Make it so we only get warnings
    alaska::set_log_level(LOG_WARN);
  }
  void TearDown() override {}

  alaska::PageManager pm;
};


TEST_F(PageManagerTest, PageManagerSanity) {
  // This test is just to make sure that the page manager is correctly initialized.
}


TEST_F(PageManagerTest, PageManagerAllocate) {
  // Test that the page manager can allocate a page
  auto page = pm.alloc_page();
  // Check that the page is not null
  ASSERT_NE(page, nullptr);
}



// test that a page manager can allocate multiple pages and they are unique
TEST_F(PageManagerTest, PageManagerAllocateMultiple) {
  // Create a set to store the pages
  std::set<void*> pages;
  // Allocate multiple pages
  for (int i = 0; i < 1000; i++) {
    auto page = pm.alloc_page();
    // Check that the page is not null
    ASSERT_NE(page, nullptr);
    // Check that the page is unique
    ASSERT_EQ(pages.count(page), 0);
    pages.insert(page);
  }
}


// test that the page manager can allocate a page, free it, and get it back from
// the free list (this ensures that the free list)
TEST_F(PageManagerTest, PageManagerFree) {
  // Allocate another page.
  auto page = pm.alloc_page();
  // Check that the page is not null
  ASSERT_NE(page, nullptr);


  // Allocate another page, to bump the bump allocator a bit.
  pm.alloc_page();

  // Free the page
  pm.free_page(page);

  // Allocate another page
  auto page2 = pm.alloc_page();
  // Check that the page is the same as the one that was freed
  ASSERT_EQ(page, page2);
}


TEST_F(PageManagerTest, PageManagerAllocateAndFree) {
  // Test that the page manager can allocate and free multiple pages
  std::vector<void*> pages;
  const int numPages = 10;

  // Allocate multiple pages
  for (int i = 0; i < numPages; i++) {
    auto page = pm.alloc_page();
    ASSERT_NE(page, nullptr);
    pages.push_back(page);
  }

  // Free the pages
  for (auto page : pages) {
    pm.free_page(page);
  }

  // Allocate the same number of pages again
  std::vector<void*> newPages;
  for (int i = 0; i < numPages; i++) {
    auto page = pm.alloc_page();
    ASSERT_NE(page, nullptr);
    newPages.push_back(page);
  }

  // Check that the newly allocated pages are different from the previously allocated ones
  for (int i = 0; i < numPages; i++) {
    ASSERT_NE(pages[i], newPages[i]);
  }
}

TEST_F(PageManagerTest, PageManagerFreeInvalidPage) {
  // Test that the page manager handles freeing an invalid page correctly
  void* invalidPage = reinterpret_cast<void*>(0x1000);

  // Try to free the invalid page
  pm.free_page(invalidPage);

  // Check that the page manager reports the correct number of allocated pages
  ASSERT_EQ(pm.get_allocated_page_count(), 0);
}


////////////////////////////////////////////////////////////////////

class HeapTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Make it so we only get warnings
    alaska::set_log_level(LOG_WARN);
  }
  void TearDown() override {}

  alaska::Heap heap;
};


TEST_F(HeapTest, HeapSanity) {
  // This test is just to make sure that the heap is correctly initialized.
}


// Test with fake pointers to HeapPages that the heap page table maps values correctly.
// This test is pretty nonsense, but it's a good way to test the page table computes bits correctly.
TEST_F(HeapTest, HeapPageTable) {
  ck::vec<void*> pages;
  for (uintptr_t i = 0; i < (1 << alaska::bits_per_pt_level) * 2; i++) {
    auto page = heap.pm.alloc_page();
    pages.push(page);
    auto hp = reinterpret_cast<alaska::HeapPage*>(i);

    heap.pt.set(page, hp);
  }
  for (uintptr_t i = 0; i < (1 << alaska::bits_per_pt_level) + 2; i++) {
    auto hp_expected = reinterpret_cast<alaska::HeapPage*>(i);
    auto page = pages[i];

    auto hp_actual = heap.pt.get(page);
    ASSERT_EQ(hp_actual, hp_expected);
  }
}