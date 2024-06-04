#include <gtest/gtest.h>
#include <alaska.h>
#include "alaska/Logger.hpp"
#include "gtest/gtest.h"
#include <vector>
#include <alaska/Heap.hpp>

#include <alaska/Runtime.hpp>


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