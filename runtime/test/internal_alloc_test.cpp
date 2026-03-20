#include <gtest/gtest.h>
#include <alaska/internal/alaska_internal_malloc.h>
#include <string.h>
#include <stdint.h>
#include <thread>
#include <vector>
#include <set>

TEST(InternalAlloc, AllocZeroReturnsNonNull) {
  void *p = alaska_internal_malloc(0);
  ASSERT_NE(p, nullptr);
  alaska_internal_free(p);
}

TEST(InternalAlloc, SmallAllocRoundTrip) {
  void *p = alaska_internal_malloc(64);
  ASSERT_NE(p, nullptr);
  memset(p, 0xAB, 64);
  alaska_internal_free(p);
}

TEST(InternalAlloc, LargeAllocRoundTrip) {
  size_t sz = 128 * 1024;
  void *p = alaska_internal_malloc(sz);
  ASSERT_NE(p, nullptr);
  memset(p, 0xCD, sz);
  alaska_internal_free(p);
}

TEST(InternalAlloc, Alignment16) {
  for (int i = 0; i < 20; i++) {
    void *p = alaska_internal_malloc(1 + i * 7);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % 16, 0u);
    alaska_internal_free(p);
  }
}

TEST(InternalAlloc, CallocZeroed) {
  void *p = alaska_internal_calloc(10, 64);
  ASSERT_NE(p, nullptr);
  auto *bytes = static_cast<unsigned char *>(p);
  for (size_t i = 0; i < 640; i++) {
    EXPECT_EQ(bytes[i], 0) << "byte " << i << " not zero";
  }
  alaska_internal_free(p);
}

TEST(InternalAlloc, MultipleAllocsUnique) {
  constexpr int N = 100;
  std::set<void *> ptrs;
  for (int i = 0; i < N; i++) {
    void *p = alaska_internal_malloc(32);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(ptrs.count(p), 0u) << "duplicate pointer returned";
    ptrs.insert(p);
  }
  for (void *p : ptrs) {
    alaska_internal_free(p);
  }
}

TEST(InternalAlloc, FreelistReuse) {
  void *p1 = alaska_internal_malloc(48);
  ASSERT_NE(p1, nullptr);
  alaska_internal_free(p1);

  void *p2 = alaska_internal_malloc(48);
  ASSERT_NE(p2, nullptr);
  // Should reuse from freelist — same address
  EXPECT_EQ(p1, p2);
  alaska_internal_free(p2);
}

TEST(InternalAlloc, LargeAllocDoesNotCorruptSmall) {
  void *small = alaska_internal_malloc(64);
  ASSERT_NE(small, nullptr);
  memset(small, 0x42, 64);

  size_t large_sz = 256 * 1024;
  void *large = alaska_internal_malloc(large_sz);
  ASSERT_NE(large, nullptr);
  memset(large, 0xFF, large_sz);

  auto *bytes = static_cast<unsigned char *>(small);
  for (int i = 0; i < 64; i++) {
    EXPECT_EQ(bytes[i], 0x42) << "small alloc corrupted at byte " << i;
  }

  alaska_internal_free(large);
  alaska_internal_free(small);
}

TEST(InternalAlloc, ThreadedStress) {
  constexpr int NUM_THREADS = 4;
  constexpr int ALLOCS_PER_THREAD = 500;

  auto worker = []() {
    std::vector<void *> ptrs;
    for (int i = 0; i < ALLOCS_PER_THREAD; i++) {
      size_t sz = 16 + (i % 200) * 8;
      void *p = alaska_internal_malloc(sz);
      ASSERT_NE(p, nullptr);
      memset(p, 0xAA, sz);
      ptrs.push_back(p);
    }
    for (void *p : ptrs) {
      alaska_internal_free(p);
    }
  };

  std::vector<std::thread> threads;
  for (int i = 0; i < NUM_THREADS; i++) {
    threads.emplace_back(worker);
  }
  for (auto &t : threads) {
    t.join();
  }
}
