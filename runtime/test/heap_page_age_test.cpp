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


TEST_F(HeapPageAgeTest, ResetAgeSetsTimestamp) {
  auto *page = heap.get_sizedpage(16);
  ASSERT_NE(page, nullptr);
  ASSERT_EQ(page->time_of_last_use, 0u);

  heap.reset_age(*page);
  ASSERT_GT(page->time_of_last_use, 0u);
}


TEST_F(HeapPageAgeTest, ResetAgeUpdatesTimestamp) {
  auto *page = heap.get_sizedpage(16);
  heap.reset_age(*page);
  auto first = page->time_of_last_use;

  // Sleep briefly so the clock advances
  std::this_thread::sleep_for(std::chrono::milliseconds(2));

  heap.reset_age(*page);
  ASSERT_GE(page->time_of_last_use, first);
}


TEST_F(HeapPageAgeTest, GetAgingPagesSkipsYoungPages) {
  auto *page = heap.get_sizedpage(16);
  heap.reset_age(*page);

  // The page was just reset, so with a 1000ms threshold it should not be aging
  int count = 0;
  heap.get_aging_pages(1000, [&](alaska::HeapPage *) { count++; });
  ASSERT_EQ(count, 0);
}


TEST_F(HeapPageAgeTest, GetAgingPagesFindsOldPages) {
  auto *page = heap.get_sizedpage(16);
  heap.reset_age(*page);

  // Sleep so the page ages past the threshold
  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  std::vector<alaska::HeapPage *> aging;
  heap.get_aging_pages(10, [&](alaska::HeapPage *p) { aging.push_back(p); });
  ASSERT_EQ(aging.size(), 1u);
  ASSERT_EQ(aging[0], page);
}


TEST_F(HeapPageAgeTest, NurseryOrderOldestAtTail) {
  // Reset three pages with small delays so timestamps differ
  auto *p1 = heap.get_sizedpage(16);
  heap.reset_age(*p1);
  std::this_thread::sleep_for(std::chrono::milliseconds(2));

  auto *p2 = heap.get_sizedpage(32);
  heap.reset_age(*p2);
  std::this_thread::sleep_for(std::chrono::milliseconds(2));

  auto *p3 = heap.get_sizedpage(64);
  heap.reset_age(*p3);

  // Wait so all are "old"
  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  // get_aging_pages iterates from oldest to youngest (tail to head)
  std::vector<alaska::HeapPage *> visited;
  heap.get_aging_pages(10, [&](alaska::HeapPage *p) { visited.push_back(p); });

  ASSERT_EQ(visited.size(), 3u);
  // p1 is oldest (reset first), should appear first in reverse iteration
  ASSERT_EQ(visited[0], p1);
  ASSERT_EQ(visited[1], p2);
  ASSERT_EQ(visited[2], p3);
}


TEST_F(HeapPageAgeTest, ReactivationMovesToNurseryFront) {
  auto *p1 = heap.get_sizedpage(16);
  heap.reset_age(*p1);
  std::this_thread::sleep_for(std::chrono::milliseconds(2));

  auto *p2 = heap.get_sizedpage(32);
  heap.reset_age(*p2);
  std::this_thread::sleep_for(std::chrono::milliseconds(2));

  // Re-activate p1 — it should move to head (youngest) in nursery
  heap.reset_age(*p1);

  // Wait so both are old
  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  std::vector<alaska::HeapPage *> visited;
  heap.get_aging_pages(10, [&](alaska::HeapPage *p) { visited.push_back(p); });

  ASSERT_EQ(visited.size(), 2u);
  // p2 is now oldest (it wasn't re-reset), p1 is youngest
  ASSERT_EQ(visited[0], p2);
  ASSERT_EQ(visited[1], p1);
}


TEST_F(HeapPageAgeTest, AgingPageMovedToElderly) {
  auto *page = heap.get_sizedpage(16);
  heap.reset_age(*page);
  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  // Explicitly promote inside the callback
  heap.get_aging_pages(10, [&](alaska::HeapPage *p) { heap.promote_to_elderly(*p); });

  // Verify the page is not reachable via the nursery
  int nursery_count = 0;
  heap.get_aging_pages(0, [&](alaska::HeapPage *) { nursery_count++; });
  ASSERT_EQ(nursery_count, 0);

  // Verify it is in m_elderly by checking the list directly
  alaska::HeapPage *entry;
  int elderly_count = 0;
  list_for_each_entry(entry, &heap.m_elderly, age_list) { elderly_count++; }
  ASSERT_EQ(elderly_count, 1);
}


TEST_F(HeapPageAgeTest, ResetAgeOnElderlyReturnsToNursery) {
  auto *page = heap.get_sizedpage(16);
  heap.reset_age(*page);
  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  // Promote page to elderly
  heap.get_aging_pages(10, [&](alaska::HeapPage *p) { heap.promote_to_elderly(*p); });

  // Simulate re-allocation: reset_age should pull it back to nursery
  heap.reset_age(*page);

  // Elderly list should now be empty
  int elderly_count = 0;
  alaska::HeapPage *entry;
  list_for_each_entry(entry, &heap.m_elderly, age_list) { elderly_count++; }
  ASSERT_EQ(elderly_count, 0);

  // Nursery should contain the page
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  std::vector<alaska::HeapPage *> found;
  heap.get_aging_pages(10, [&](alaska::HeapPage *p) { found.push_back(p); });
  ASSERT_EQ(found.size(), 1u);
  ASSERT_EQ(found[0], page);
}


// Callback promotes only the oldest page; the rest must stay in nursery.
TEST_F(HeapPageAgeTest, PartialPromotion) {
  auto *p1 = heap.get_sizedpage(16);
  heap.reset_age(*p1);
  std::this_thread::sleep_for(std::chrono::milliseconds(2));

  auto *p2 = heap.get_sizedpage(32);
  heap.reset_age(*p2);
  std::this_thread::sleep_for(std::chrono::milliseconds(2));

  auto *p3 = heap.get_sizedpage(64);
  heap.reset_age(*p3);

  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  // Only promote the first page visited (p1, the oldest)
  int seen = 0;
  heap.get_aging_pages(10, [&](alaska::HeapPage *p) {
    if (seen++ == 0) heap.promote_to_elderly(*p);
  });

  // One page in elderly
  int elderly_count = 0;
  alaska::HeapPage *e;
  list_for_each_entry(e, &heap.m_elderly, age_list) { elderly_count++; }
  ASSERT_EQ(elderly_count, 1);

  // Two pages still in nursery
  std::vector<alaska::HeapPage *> nursery_pages;
  heap.get_aging_pages(10, [&](alaska::HeapPage *p) { nursery_pages.push_back(p); });
  ASSERT_EQ(nursery_pages.size(), 2u);
}


// Callback never promotes; all pages stay in nursery across multiple calls.
TEST_F(HeapPageAgeTest, NoPromotionLeavesNurseryIntact) {
  auto *p1 = heap.get_sizedpage(16);
  auto *p2 = heap.get_sizedpage(32);
  heap.reset_age(*p1);
  heap.reset_age(*p2);
  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  // Two calls without promoting
  for (int i = 0; i < 2; i++) {
    int count = 0;
    heap.get_aging_pages(10, [&](alaska::HeapPage *) { count++; });
    ASSERT_EQ(count, 2);
  }

  // Elderly remains empty
  int elderly_count = 0;
  alaska::HeapPage *e;
  list_for_each_entry(e, &heap.m_elderly, age_list) { elderly_count++; }
  ASSERT_EQ(elderly_count, 0);
}


// Verify nursery → elderly → nursery → elderly across two full cycles.
TEST_F(HeapPageAgeTest, MultiCyclePromotion) {
  auto *page = heap.get_sizedpage(16);

  for (int cycle = 0; cycle < 2; cycle++) {
    heap.reset_age(*page);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    int seen = 0;
    heap.get_aging_pages(10, [&](alaska::HeapPage *p) {
      seen++;
      heap.promote_to_elderly(*p);
    });
    ASSERT_EQ(seen, 1);

    // Page is in elderly; reset_age cleared it from the previous cycle's elderly list
    int elderly_count = 0;
    alaska::HeapPage *e;
    list_for_each_entry(e, &heap.m_elderly, age_list) { elderly_count++; }
    ASSERT_EQ(elderly_count, 1);
  }
}


// Zero threshold: every page in the nursery is immediately aged out.
TEST_F(HeapPageAgeTest, ZeroThresholdAgesAllPages) {
  auto *p1 = heap.get_sizedpage(16);
  auto *p2 = heap.get_sizedpage(32);
  auto *p3 = heap.get_sizedpage(64);
  heap.reset_age(*p1);
  heap.reset_age(*p2);
  heap.reset_age(*p3);

  // No sleep — zero threshold must match everything
  int count = 0;
  heap.get_aging_pages(0, [&](alaska::HeapPage *p) {
    count++;
    heap.promote_to_elderly(*p);
  });
  ASSERT_EQ(count, 3);

  int elderly_count = 0;
  alaska::HeapPage *e;
  list_for_each_entry(e, &heap.m_elderly, age_list) { elderly_count++; }
  ASSERT_EQ(elderly_count, 3);
}


// Elderly accumulates pages from multiple get_aging_pages calls.
TEST_F(HeapPageAgeTest, ElderlyAccumulatesAcrossCalls) {
  auto *p1 = heap.get_sizedpage(16);
  heap.reset_age(*p1);
  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  auto *p2 = heap.get_sizedpage(32);
  heap.reset_age(*p2);
  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  // First call: promotes p1
  heap.get_aging_pages(10, [&](alaska::HeapPage *p) { heap.promote_to_elderly(*p); });

  // Second call: promotes p2
  heap.get_aging_pages(10, [&](alaska::HeapPage *p) { heap.promote_to_elderly(*p); });

  int elderly_count = 0;
  alaska::HeapPage *e;
  list_for_each_entry(e, &heap.m_elderly, age_list) { elderly_count++; }
  ASSERT_EQ(elderly_count, 2);

  int nursery_count = 0;
  heap.get_aging_pages(0, [&](alaska::HeapPage *) { nursery_count++; });
  ASSERT_EQ(nursery_count, 0);
}


// Un-promoted pages retain their relative order in the nursery.
TEST_F(HeapPageAgeTest, SkippedPagesRetainNurseryOrder) {
  auto *p1 = heap.get_sizedpage(16);
  heap.reset_age(*p1);
  std::this_thread::sleep_for(std::chrono::milliseconds(2));

  auto *p2 = heap.get_sizedpage(32);
  heap.reset_age(*p2);
  std::this_thread::sleep_for(std::chrono::milliseconds(2));

  auto *p3 = heap.get_sizedpage(64);
  heap.reset_age(*p3);

  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  // Promote only p2 (the middle page)
  heap.get_aging_pages(10, [&](alaska::HeapPage *p) {
    if (p == p2) heap.promote_to_elderly(*p);
  });

  // p1 and p3 should still appear in oldest-first order
  std::vector<alaska::HeapPage *> remaining;
  heap.get_aging_pages(10, [&](alaska::HeapPage *p) { remaining.push_back(p); });
  ASSERT_EQ(remaining.size(), 2u);
  ASSERT_EQ(remaining[0], p1);
  ASSERT_EQ(remaining[1], p3);
}
