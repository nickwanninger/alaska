#include <gtest/gtest.h>
#include <alaska.h>
#include "alaska/util/Logger.hpp"
#include "gtest/gtest.h"
#include <vector>
#include <gmock/gmock.h>
#include <alaska/heaps/Heap.hpp>

#include <alaska/core/Runtime.hpp>
#include <alaska/util/handle_ptr.hpp>

class BarrierWorkerTest : public ::testing::Test {
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


class MockBarrierWorker : public alaska::BarrierWorker {
 public:
  MockBarrierWorker() = default;
  virtual ~MockBarrierWorker() = default;

  MOCK_METHOD(alaska::BarrierWorkResult, barrier_work, (), (override));
};


TEST_F(BarrierWorkerTest, Sanity) {
  // This test is just to make sure that the runtime is correctly initialized.
}


TEST_F(BarrierWorkerTest, NothingRegisteredCountZero) {
  ASSERT_EQ(runtime.barrier_work_count(), 0);
}


TEST_F(BarrierWorkerTest, RegistrationCounted) {
  MockBarrierWorker worker;
  worker.register_barrier_work();
  ASSERT_EQ(runtime.barrier_work_count(), 1);
}


TEST_F(BarrierWorkerTest, RegistrationCountedDestructorDeregisters) {
  {
    MockBarrierWorker worker;
    worker.register_barrier_work();
    ASSERT_EQ(runtime.barrier_work_count(), 1);
  }
  ASSERT_EQ(runtime.barrier_work_count(), 0);
}


TEST_F(BarrierWorkerTest, UnregisteredBarrierWorkerIsNotCalled) {
  MockBarrierWorker worker;
  EXPECT_CALL(worker, barrier_work()).Times(0);

  runtime.with_barrier([&]() {
  });
}


TEST_F(BarrierWorkerTest, BarrierWorkerIsCalled) {
  MockBarrierWorker worker;
  worker.register_barrier_work();
  EXPECT_CALL(worker, barrier_work()).Times(1);
  runtime.with_barrier([&]() {
    ASSERT_EQ(runtime.barrier_work_count(), 1);
  });
  ASSERT_EQ(runtime.barrier_work_count(), 1);
}