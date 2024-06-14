#include <gtest/gtest.h>
#include <alaska.h>
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
