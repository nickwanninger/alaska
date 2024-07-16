#include <gtest/gtest.h>
#include <alaska.h>
#include "alaska/Logger.hpp"
#include "alaska/SizeClass.hpp"
#include "gtest/gtest.h"
#include <vector>
#include <alaska/Heap.hpp>

#include <alaska/Runtime.hpp>

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



TEST_F(HeapTest, SizedPageGet) {
  auto sp = heap.get_sizedpage(16);
  ASSERT_NE(sp, nullptr);
}



TEST_F(HeapTest, SizedPageGetPutGet) {
  // Allocating a heap page, then putting it back should return it again.
  auto sp = heap.get_sizedpage(16);
  ASSERT_NE(sp, nullptr);
  heap.put_sizedpage(sp);
  auto sp2 = heap.get_sizedpage(16);
  ASSERT_EQ(sp, sp2);
}



// TEST_F(HeapTest, SizedPageMany) {
//   for (int i = 0; i < alaska::num_size_classes - 1; i++) {
//     printf("cls = %d\n", i);
//     heap.get_sizedpage(alaska::class_to_size(i));
//   }
// }
