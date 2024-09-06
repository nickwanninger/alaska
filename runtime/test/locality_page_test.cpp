#include <gtest/gtest.h>
#include <alaska.h>
#include "alaska/Logger.hpp"
#include "gtest/gtest.h"
#include <vector>
#include <alaska/Heap.hpp>

#include <alaska/Runtime.hpp>
#include <alaska/sim/handle_ptr.hpp>

#define DUMMY_THREADCACHE ((alaska::ThreadCache *)0x1000UL)

class LocalityPageTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Make it so we only get warnings
    alaska::set_log_level(LOG_WARN);
    tc = runtime.new_threadcache();
  }
  void TearDown() override { runtime.del_threadcache(tc); }

  alaska::ThreadCache *tc;
  alaska::Runtime runtime;
};




TEST_F(LocalityPageTest, Sanity) {
  // This test is just to make sure that the runtime is correctly initialized.
}


TEST_F(LocalityPageTest, LocalizeInvalid) {
  // It should be invalid to attempt to localize a non-handle
  int value = 0;
  ASSERT_FALSE(tc->localize((void *)&value, runtime.localization_epoch));
}


TEST_F(LocalityPageTest, LocalizeAllocation) {
  // Allocate a handle, then localize it.
  alaska::sim::handle_ptr<int> h = (int *)tc->halloc(32);
  ASSERT_NE(h.get(), nullptr);

  void *orig_location = h.translate();
  ASSERT_NE(orig_location, nullptr);

  *h = 42;
  ASSERT_EQ(*h, 42);

  bool loc = tc->localize(h.get(), runtime.localization_epoch);
  ASSERT_TRUE(loc);

  void *new_location = h.translate();
  ASSERT_NE(new_location, nullptr);
  ASSERT_NE(new_location, orig_location);

  ASSERT_EQ(*h, 42);
}
