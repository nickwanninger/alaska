#include <gtest/gtest.h>
#include <alaska.h>
#include "alaska/Configuration.hpp"
#include "alaska/util/Logger.hpp"
#include "alaska/heaps/SizeClass.hpp"
#include "gtest/gtest.h"
#include <vector>
#include <alaska/heaps/Heap.hpp>

#include <alaska/core/Runtime.hpp>


static alaska::Configuration g_config;
class HeapTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Make it so we only get warnings
    alaska::set_log_level(LOG_WARN);
  }
  void TearDown() override {}

  alaska::Heap heap{g_config};
};


TEST_F(HeapTest, HeapSanity) {
  // This test is just to make sure that the heap is correctly initialized.
}




TEST_F(HeapTest, SizedPageGet) {
  auto sp = heap.get_sizedpage(16);
  ASSERT_NE(sp, nullptr);
}


