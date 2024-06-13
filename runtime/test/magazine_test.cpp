#include <gtest/gtest.h>
#include <gmock/gmock.h>


#include <alaska.h>
#include <alaska/Logger.hpp>
#include <vector>


#include <alaska/HeapPage.hpp>
#include <alaska/Heap.hpp>
#include <alaska/Magazine.hpp>
#include <alaska/Runtime.hpp>


class MockHeapPage : public alaska::HeapPage {
 public:
  ~MockHeapPage(void) override {}
  MOCK_METHOD(void*, alloc, (const alaska::Mapping& m, alaska::AlignedSize size), (override));
  MOCK_METHOD(bool, release, (alaska::Mapping& m, void* ptr), (override));
};

class MagazineTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Make it so we only get warnings
    alaska::set_log_level(LOG_WARN);
  }
  void TearDown() override {}

  alaska::HeapPage* new_page(void) {
    return new MockHeapPage();
  }

  alaska::Magazine mag;
};


TEST_F(MagazineTest, HeapSanity) {
  // This test is just to make sure that the heap is correctly initialized.
}



// Test that adding a page increments the count
TEST_F(MagazineTest, Add) {
  alaska::HeapPage* page = new_page();
  mag.add(page);

  ASSERT_EQ(1, mag.size());
}

// Test that adding two pages increments the count by two
TEST_F(MagazineTest, AddTwo) {
  alaska::HeapPage* page1 = new_page();
  alaska::HeapPage* page2 = new_page();
  mag.add(page1);
  mag.add(page2);

  ASSERT_EQ(2, mag.size());
}


TEST_F(MagazineTest, AddRemove) {
  alaska::HeapPage* page = new_page();
  mag.add(page);
  alaska::HeapPage *popped = mag.pop();

  ASSERT_EQ(page, popped);
}


// Add two pages, remove one, and test that the count is one
TEST_F(MagazineTest, AddTwoRemoveOne) {
  alaska::HeapPage* page1 = new_page();
  alaska::HeapPage* page2 = new_page();
  mag.add(page1);
  mag.add(page2);
  alaska::HeapPage *popped = mag.pop();

  ASSERT_EQ(1, mag.size());
  ASSERT_EQ(page1, popped);
}

// Add two pages, remove one, and test that pop returns the other
TEST_F(MagazineTest, AddTwoRemoveOnePop) {
  alaska::HeapPage* page1 = new_page();
  alaska::HeapPage* page2 = new_page();
  mag.add(page1);
  mag.add(page2);
  alaska::HeapPage *popped = mag.pop();

  ASSERT_EQ(page1, popped);
}
