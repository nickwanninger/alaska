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
  // Allocating a locality page then putting it back should return it
  // again to promote reuse. Asserting this might be restrictive on
  // future policy, but for now it's good enough.
  auto sp = heap.get_sizedpage(16);
  ASSERT_NE(sp, nullptr);
  heap.put_page(sp);
  auto sp2 = heap.get_sizedpage(16);
  ASSERT_EQ(sp, sp2);
}


TEST_F(HeapTest, LocalityPageGet) {
  size_t size_req = 32;
  auto lp = heap.get_localitypage(size_req);
  ASSERT_NE(lp, nullptr);
  alaska::Mapping m;
  ASSERT_NE(lp->alloc(m, 32), nullptr);

}



TEST_F(HeapTest, LocalityGetPutGet) {
  size_t size_req = 32;
  // Allocating a locality page then putting it back should return it
  // again to promote reuse. Asserting this might be restrictive on
  // future policy, but for now it's good enough.
  auto lp = heap.get_localitypage(size_req);
  ASSERT_NE(lp, nullptr);
  heap.put_page(lp);

  auto lp2 = heap.get_localitypage(size_req);
  ASSERT_NE(lp2, nullptr);
  ASSERT_EQ(lp, lp2);
}
