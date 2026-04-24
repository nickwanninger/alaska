#include <gtest/gtest.h>

#include <alaska/core/Runtime.hpp>
#include <alaska/core/ThreadCache.hpp>
#include <alaska/heaps/Heap.hpp>

namespace {
  class SwapTest : public ::testing::Test {
   protected:
    void SetUp() override {
      alaska::Configuration config;
      config.swap_enabled = true;
      config.swap_use_memory_disk = true;
      rt = new alaska::Runtime(config);
      producer = rt->new_threadcache();
      actor = rt->new_threadcache();
    }

    void TearDown() override {
      rt->del_threadcache(producer);
      rt->del_threadcache(actor);
      delete rt;
    }

    alaska::Runtime *rt = nullptr;
    alaska::ThreadCache *producer = nullptr;
    alaska::ThreadCache *actor = nullptr;
  };

  TEST_F(SwapTest, SwappedObjectStillReportsItsSize) {
    void *handle = producer->halloc(16);
    ASSERT_NE(handle, nullptr);

    auto *mapping = alaska::Mapping::from_handle_safe(handle);
    ASSERT_NE(mapping, nullptr);

    ASSERT_TRUE(rt->with_barrier([&]() {
      auto *swap = rt->get_swap_space();
      ASSERT_NE(swap, nullptr);
      EXPECT_TRUE(swap->swap_out(mapping));
    }));

    EXPECT_TRUE(mapping->fault_pending());
    EXPECT_EQ(producer->get_size(handle), 16u);
  }

  TEST_F(SwapTest, FaultingOneRecordRestoresTheEntireSwapPageCohort) {
    void *h1 = producer->halloc(16);
    void *h2 = producer->halloc(16);
    ASSERT_NE(h1, nullptr);
    ASSERT_NE(h2, nullptr);

    auto *m1 = alaska::Mapping::from_handle_safe(h1);
    auto *m2 = alaska::Mapping::from_handle_safe(h2);
    ASSERT_NE(m1, nullptr);
    ASSERT_NE(m2, nullptr);

    auto *p1 = static_cast<uint64_t *>(alaska::Mapping::translate(h1));
    auto *p2 = static_cast<uint64_t *>(alaska::Mapping::translate(h2));
    p1[0] = 0x1111111111111111ULL;
    p1[1] = 0x2222222222222222ULL;
    p2[0] = 0x3333333333333333ULL;
    p2[1] = 0x4444444444444444ULL;

    ASSERT_TRUE(rt->with_barrier([&]() {
      auto *swap = rt->get_swap_space();
      ASSERT_NE(swap, nullptr);
      EXPECT_TRUE(swap->swap_out(m1));
      EXPECT_TRUE(swap->swap_out(m2));
    }));

    ASSERT_TRUE(m1->fault_pending());
    ASSERT_TRUE(m2->fault_pending());

    auto *swap = rt->get_swap_space();
    ASSERT_NE(swap, nullptr);
    EXPECT_TRUE(swap->handle_fault(m1, actor));

    EXPECT_FALSE(m1->fault_pending());
    EXPECT_FALSE(m2->fault_pending());

    auto *rp1 = static_cast<uint64_t *>(alaska::Mapping::translate(h1));
    auto *rp2 = static_cast<uint64_t *>(alaska::Mapping::translate(h2));
    ASSERT_NE(rp1, nullptr);
    ASSERT_NE(rp2, nullptr);

    EXPECT_EQ(rp1[0], 0x1111111111111111ULL);
    EXPECT_EQ(rp1[1], 0x2222222222222222ULL);
    EXPECT_EQ(rp2[0], 0x3333333333333333ULL);
    EXPECT_EQ(rp2[1], 0x4444444444444444ULL);

    EXPECT_TRUE(alaska::Heap::get_page(rp1)->is_owned_by(actor));
    EXPECT_TRUE(alaska::Heap::get_page(rp2)->is_owned_by(actor));
  }

  TEST_F(SwapTest, HfreeDiscardSwappedObjectWithoutFaultingItBackIn) {
    void *handle = producer->halloc(16);
    ASSERT_NE(handle, nullptr);

    auto *mapping = alaska::Mapping::from_handle_safe(handle);
    ASSERT_NE(mapping, nullptr);

    ASSERT_TRUE(rt->with_barrier([&]() {
      auto *swap = rt->get_swap_space();
      ASSERT_NE(swap, nullptr);
      EXPECT_TRUE(swap->swap_out(mapping));
    }));
    ASSERT_TRUE(mapping->fault_pending());

    actor->hfree(handle);

    EXPECT_FALSE(mapping->fault_pending());
    auto *swap = rt->get_swap_space();
    ASSERT_NE(swap, nullptr);
    EXPECT_FALSE(swap->handle_fault(mapping, actor));
  }
}
