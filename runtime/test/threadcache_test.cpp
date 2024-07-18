#include <gtest/gtest.h>
#include <alaska.h>
#include "alaska/HeapPage.hpp"
#include "alaska/Logger.hpp"
#include "gtest/gtest.h"
#include <vector>
#include <alaska/Heap.hpp>

#include <alaska/ThreadCache.hpp>
#include <alaska/Runtime.hpp>

class ThreadCacheTest : public ::testing::Test {
 public:
  void SetUp() override {
    t1 = rt.new_threadcache();
    t2 = rt.new_threadcache();
  }
  void TearDown() override {
    rt.del_threadcache(t1);
    rt.del_threadcache(t2);
  }


  // We need a runtime here to manage things like the heap, and the
  // construction and management of threadcaches.
  alaska::Runtime rt;

  // The tests each have two thread caches. Note we don't have two
  // threads in this test fixture, as thread caches are technically
  // not directly associated with threads in the core runtime.
  // Instead, we just have two thread caches which we treat as two
  // different threads logically
  alaska::ThreadCache *t1, *t2;
};



TEST_F(ThreadCacheTest, Sanity) {
  // This test is just to make sure that the runtime and thread caches
  // are initialized correctly.
}

TEST_F(ThreadCacheTest, Halloc) {
  void *h = t1->halloc(16);
  ASSERT_NE(h, nullptr);
}

TEST_F(ThreadCacheTest, HallocUnique) {
  void *h1 = t1->halloc(16);
  void *h2 = t1->halloc(16);
  ASSERT_NE(h1, h2);
}


TEST_F(ThreadCacheTest, HallocFreeHallocLocal) {
  size_t size = 16;
  // Allocate one handle
  void *h1 = t1->halloc(size);
  // Translate it...
  void *p1 = alaska::Mapping::translate(h1);
  // Free it LOCALLY
  t1->hfree(h1);

  // Then allocate another handle. The threadcache should return the same backing memory
  void *h2 = t1->halloc(size);
  // Translate it to get the backing address
  void *p2 = alaska::Mapping::translate(h2);
  // The old (UAF) backing address should equal the new one.
  ASSERT_EQ(p1, p2);
  // And the handles should be the same... of course
  ASSERT_EQ(h1, h2);

  // NOTE: this test only makes sense in this controlled environment.
}



TEST_F(ThreadCacheTest, HallocFreeHallocRemote) {
  size_t size = 16;
  // Allocate one handle
  void *h1 = t1->halloc(size);
  // Translate it...
  void *p1 = alaska::Mapping::translate(h1);

  // Free it REMOTELY (on t2)
  t2->hfree(h1);

  // Then allocate another handle. The threadcache should return the same backing memory
  void *h2 = t1->halloc(size);
  // Translate it to get the backing address
  void *p2 = alaska::Mapping::translate(h2);

  // The old (UAF) backing address should not be the same because of the remote free.
  ASSERT_NE(p1, p2);
  // TODO: we currently don't care about handle equality here

  // NOTE: this test only makes sense in this controlled environment.
}




// Test that the thread cache can allocate 1M objects
TEST_F(ThreadCacheTest, Halloc1M) {
  size_t size = 64;
  for (int i = 0; i < 1 << 20; i++) {
    void *h = t1->halloc(size);
    ASSERT_NE(h, nullptr);
  }
}



// Test that the thread cache can allocate a large object
TEST_F(ThreadCacheTest, HallocHuge) {
  void *h = t1->halloc(1LU << 23);
  ASSERT_NE(h, nullptr);
}



// A few tests that validate that threadcache hrealloc works correctly and produces objects of the
// right size
TEST_F(ThreadCacheTest, Hrealloc) {
  void *h = t1->halloc(16);
  void *h2 = t1->hrealloc(h, 32);
  ASSERT_NE(h2, nullptr);
}

TEST_F(ThreadCacheTest, HreallocSize) {
  void *h = t1->halloc(16);
  void *h2 = t1->hrealloc(h, 32);
  ASSERT_EQ(t1->get_size(h2), 32);
}

TEST_F(ThreadCacheTest, HreallocHandleToHandle) {
  void *h = t1->halloc(16);
  void *h2 = t1->hrealloc(h, 32);
  ASSERT_EQ(h, h2);
}
TEST_F(ThreadCacheTest, HreallocHandleToHuge) {
  void *h = t1->halloc(16);
  void *h2 = t1->hrealloc(h, alaska::huge_object_thresh);
  // The values should not be the same
  ASSERT_NE(h, h2);
  // The new object should be a huge object (not a handle)
  ASSERT_EQ(nullptr, alaska::Mapping::from_handle_safe(h2));
}

TEST_F(ThreadCacheTest, HreallocHugeToHuge) {
  void *h = t1->halloc(alaska::huge_object_thresh + 4096);
  // The old object should be a huge object (not a handle)
  ASSERT_EQ(nullptr, alaska::Mapping::from_handle_safe(h));
  void *h2 = t1->hrealloc(h, alaska::huge_object_thresh);
  // The values should not be the same
  ASSERT_NE(h, h2);
  // The new object should be a huge object (not a handle)
  ASSERT_EQ(nullptr, alaska::Mapping::from_handle_safe(h2));
}

TEST_F(ThreadCacheTest, HreallocHugeToHandle) {
  void *h = t1->halloc(alaska::huge_object_thresh + 4096);
  // The old object should be a huge object (not a handle)
  ASSERT_EQ(nullptr, alaska::Mapping::from_handle_safe(h));

  void *h2 = t1->hrealloc(h, 32);
  // The values should not be the same
  ASSERT_NE(h, h2);
  // The new object should be a handle
  ASSERT_NE(nullptr, alaska::Mapping::from_handle_safe(h2));
}
