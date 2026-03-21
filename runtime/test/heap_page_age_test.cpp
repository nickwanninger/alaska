#include <gtest/gtest.h>
#include <alaska/heaps/Heap.hpp>
#include <alaska/Configuration.hpp>
#include <alaska/util/Logger.hpp>
#include <vector>
#include <thread>
#include <chrono>


static alaska::Configuration g_config;

class HeapPageAgeTest : public ::testing::Test {
 public:
  void SetUp() override { alaska::set_log_level(LOG_WARN); }
  void TearDown() override {}

  alaska::Heap heap{g_config};
};


TEST_F(HeapPageAgeTest, RotateOutSetsTimestamp) {
  auto *page = heap.get_sizedpage(16);
  ASSERT_NE(page, nullptr);
  ASSERT_EQ(page->time_of_last_use, 0u);

  heap.rotate_out(*page);
  ASSERT_GT(page->time_of_last_use, 0u);
}


TEST_F(HeapPageAgeTest, RotateOutUpdatesTimestamp) {
  auto *page = heap.get_sizedpage(16);
  heap.rotate_out(*page);
  auto first = page->time_of_last_use;

  // Sleep briefly so the clock advances
  std::this_thread::sleep_for(std::chrono::milliseconds(2));

  heap.rotate_out(*page);
  ASSERT_GE(page->time_of_last_use, first);
}


TEST_F(HeapPageAgeTest, ForEachOldPageSkipsYoungPages) {
  auto *page = heap.get_sizedpage(16);
  heap.rotate_out(*page);

  // The page was just rotated, so with a 1000ms threshold it should not be "old"
  int count = 0;
  heap.for_each_old_page(1000, [&](alaska::HeapPage *) { count++; });
  ASSERT_EQ(count, 0);
}


TEST_F(HeapPageAgeTest, ForEachOldPageFindsOldPages) {
  auto *page = heap.get_sizedpage(16);
  heap.rotate_out(*page);

  // Sleep so the page ages past the threshold
  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  std::vector<alaska::HeapPage *> old_pages;
  heap.for_each_old_page(10, [&](alaska::HeapPage *p) { old_pages.push_back(p); });
  ASSERT_EQ(old_pages.size(), 1u);
  ASSERT_EQ(old_pages[0], page);
}


TEST_F(HeapPageAgeTest, AgeListOrderOldestAtTail) {
  // Rotate three pages with small delays so timestamps differ
  auto *p1 = heap.get_sizedpage(16);
  heap.rotate_out(*p1);
  std::this_thread::sleep_for(std::chrono::milliseconds(2));

  auto *p2 = heap.get_sizedpage(32);
  heap.rotate_out(*p2);
  std::this_thread::sleep_for(std::chrono::milliseconds(2));

  auto *p3 = heap.get_sizedpage(64);
  heap.rotate_out(*p3);

  // Wait so all are "old"
  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  // for_each_old_page iterates from oldest to youngest (tail to head)
  std::vector<alaska::HeapPage *> visited;
  heap.for_each_old_page(10, [&](alaska::HeapPage *p) { visited.push_back(p); });

  ASSERT_EQ(visited.size(), 3u);
  // p1 is oldest (rotated first), should appear first in reverse iteration
  ASSERT_EQ(visited[0], p1);
  ASSERT_EQ(visited[1], p2);
  ASSERT_EQ(visited[2], p3);
}


TEST_F(HeapPageAgeTest, ReactivationMovesToHead) {
  auto *p1 = heap.get_sizedpage(16);
  heap.rotate_out(*p1);
  std::this_thread::sleep_for(std::chrono::milliseconds(2));

  auto *p2 = heap.get_sizedpage(32);
  heap.rotate_out(*p2);
  std::this_thread::sleep_for(std::chrono::milliseconds(2));

  // Re-activate p1 — it should move to head (youngest)
  heap.rotate_out(*p1);

  // Wait so both are old
  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  std::vector<alaska::HeapPage *> visited;
  heap.for_each_old_page(10, [&](alaska::HeapPage *p) { visited.push_back(p); });

  ASSERT_EQ(visited.size(), 2u);
  // p2 is now oldest (it wasn't re-rotated), p1 is youngest
  ASSERT_EQ(visited[0], p2);
  ASSERT_EQ(visited[1], p1);
}
