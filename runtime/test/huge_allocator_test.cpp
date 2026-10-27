#include <gtest/gtest.h>
#include <alaska/heaps/HugeAllocator.hpp>
#include <string.h>
#include <thread>
#include <vector>

class HugeAllocatorTest : public ::testing::Test {
 public:
  alaska::HugeAllocator allocator;
};


TEST_F(HugeAllocatorTest, BasicAllocFree) {
  void *p = allocator.alloc(4096);
  ASSERT_NE(p, nullptr);
  memset(p, 0xAB, 4096);
  allocator.free(p);
}

TEST_F(HugeAllocatorTest, AllocReturnsAligned) {
  void *p = allocator.alloc(5000);
  ASSERT_NE(p, nullptr);
  // The returned pointer should be after the HugeHeader, which is page-aligned + sizeof(HugeHeader).
  // At minimum it should be 8-byte aligned.
  EXPECT_EQ((uintptr_t)p % 8, 0u);
  allocator.free(p);
}

TEST_F(HugeAllocatorTest, UsableSize) {
  void *p = allocator.alloc(4096);
  ASSERT_NE(p, nullptr);
  size_t usable = allocator.usable_size(p);
  // usable_size should be >= requested size (page-aligned minus header)
  EXPECT_GE(usable, 4096u);
  allocator.free(p);
}

TEST_F(HugeAllocatorTest, UsableSizeLarge) {
  size_t req = 512 * 1024;
  void *p = allocator.alloc(req);
  ASSERT_NE(p, nullptr);
  EXPECT_GE(allocator.usable_size(p), req);
  allocator.free(p);
}

TEST_F(HugeAllocatorTest, MultipleSmallHuge) {
  std::vector<void *> ptrs;
  for (int i = 0; i < 100; i++) {
    void *p = allocator.alloc(4096 + i * 16);
    ASSERT_NE(p, nullptr);
    memset(p, (uint8_t)i, 4096);
    ptrs.push_back(p);
  }
  // All pointers should be unique.
  for (size_t i = 0; i < ptrs.size(); i++) {
    for (size_t j = i + 1; j < ptrs.size(); j++) {
      EXPECT_NE(ptrs[i], ptrs[j]);
    }
  }
  for (auto *p : ptrs) {
    allocator.free(p);
  }
}

TEST_F(HugeAllocatorTest, FreeAndReuse) {
  void *p1 = allocator.alloc(4096);
  ASSERT_NE(p1, nullptr);
  allocator.free(p1);

  void *p2 = allocator.alloc(4096);
  ASSERT_NE(p2, nullptr);
  allocator.free(p2);
  // After free + re-alloc of same size, the allocator should reuse space.
  // We don't assert p1 == p2 since madvise may change things, but it should work.
}

TEST_F(HugeAllocatorTest, DirectMmapPath) {
  // > 256KB should go through direct mmap
  size_t big = 512 * 1024;
  void *p = allocator.alloc(big);
  ASSERT_NE(p, nullptr);
  memset(p, 0xCD, big);
  EXPECT_GE(allocator.usable_size(p), big);
  allocator.free(p);
}

TEST_F(HugeAllocatorTest, ZeroSizeReturnsNull) {
  void *p = allocator.alloc(0);
  EXPECT_EQ(p, nullptr);
}

TEST_F(HugeAllocatorTest, FreeNull) {
  // Should not crash.
  allocator.free(nullptr);
}

TEST_F(HugeAllocatorTest, OwnsPageRun) {
  void *p = allocator.alloc(8192);
  ASSERT_NE(p, nullptr);
  EXPECT_TRUE(allocator.owns(p));
  EXPECT_TRUE(allocator.free(p));
}

TEST_F(HugeAllocatorTest, OwnsDirectMmap) {
  void *p = allocator.alloc(512 * 1024);
  ASSERT_NE(p, nullptr);
  EXPECT_TRUE(allocator.owns(p));
  EXPECT_TRUE(allocator.free(p));
  EXPECT_FALSE(allocator.owns(p));
}

TEST_F(HugeAllocatorTest, OwnsNull) {
  EXPECT_FALSE(allocator.owns(nullptr));
}

// A pointer from glibc has no HugeHeader. free() must not touch it.
TEST_F(HugeAllocatorTest, ForeignPointerNotOwned) {
  char *s = strdup("foreign");
  ASSERT_NE(s, nullptr);
  EXPECT_FALSE(allocator.owns(s));
  EXPECT_FALSE(allocator.free(s));
  EXPECT_STREQ(s, "foreign");
  ::free(s);
}

// Each alignment, with sizes on the page-run and direct-mmap paths.
TEST_F(HugeAllocatorTest, AllocAligned) {
  size_t aligns[] = {16, 32, 64, 256, 4096, 8192, 1 << 16, 1 << 21};
  size_t sizes[] = {1, 100, 4096, 5000, 300 * 1024};
  for (size_t align : aligns) {
    for (size_t size : sizes) {
      void *p = allocator.alloc_aligned(size, align);
      ASSERT_NE(p, nullptr) << "align " << align << " size " << size;
      EXPECT_EQ((uintptr_t)p % align, 0u) << "align " << align << " size " << size;
      EXPECT_GE(allocator.usable_size(p), size);
      memset(p, 0x5A, allocator.usable_size(p));
      EXPECT_TRUE(allocator.owns(p));
      EXPECT_TRUE(allocator.free(p));
    }
  }
}

TEST_F(HugeAllocatorTest, AllocTooLargeReturnsNull) {
  EXPECT_EQ(allocator.alloc(SIZE_MAX - 100), nullptr);
  EXPECT_EQ(allocator.alloc_aligned(SIZE_MAX - 100, 64), nullptr);
  EXPECT_EQ(allocator.alloc_aligned(100, (SIZE_MAX >> 1) + 1), nullptr);
}

// A freed aligned run must be reusable by a plain allocation.
TEST_F(HugeAllocatorTest, AllocAlignedReuse) {
  void *p = allocator.alloc_aligned(100, 4096);
  ASSERT_NE(p, nullptr);
  EXPECT_TRUE(allocator.free(p));
  void *q = allocator.alloc(8192);
  ASSERT_NE(q, nullptr);
  EXPECT_TRUE(allocator.free(q));
}

TEST_F(HugeAllocatorTest, WriteAndReadBack) {
  size_t sz = 8192;
  void *p = allocator.alloc(sz);
  ASSERT_NE(p, nullptr);
  memset(p, 0x42, sz);
  for (size_t i = 0; i < sz; i++) {
    EXPECT_EQ(((uint8_t *)p)[i], 0x42);
  }
  allocator.free(p);
}

TEST_F(HugeAllocatorTest, ThreadedAllocFree) {
  constexpr int num_threads = 4;
  constexpr int allocs_per_thread = 50;

  auto worker = [&]() {
    std::vector<void *> ptrs;
    for (int i = 0; i < allocs_per_thread; i++) {
      void *p = allocator.alloc(4096 + i * 64);
      ASSERT_NE(p, nullptr);
      memset(p, 0xFF, 4096);
      ptrs.push_back(p);
    }
    for (auto *p : ptrs) {
      allocator.free(p);
    }
  };

  std::vector<std::thread> threads;
  for (int i = 0; i < num_threads; i++) {
    threads.emplace_back(worker);
  }
  for (auto &t : threads) {
    t.join();
  }
}
