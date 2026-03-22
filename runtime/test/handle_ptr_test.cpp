#include <gtest/gtest.h>
#include <gtest/gtest.h>

#include <alaska.h>
#include <alaska/util/Logger.hpp>
#include <vector>
#include <alaska/heaps/Heap.hpp>
#include <alaska/core/Runtime.hpp>
#include "alaska/core/ThreadCache.hpp"
#include <alaska/util/handle_ptr.hpp>


class HandlePtrTest : public ::testing::Test {
 public:
  void SetUp() override { tc = runtime.new_threadcache(); }

  void TearDown() override {
    runtime.del_threadcache(tc);
    tc = nullptr;
  }


  template <typename T>
  alaska::sim::handle_ptr<T> alloc(void) {
    return (T *)tc->halloc(sizeof(T));
  }



  template <typename T>
  void release(alaska::sim::handle_ptr<T> h) {
    tc->hfree(h);
  }

  alaska::ThreadCache *tc;
  alaska::Runtime runtime;
};



TEST_F(HandlePtrTest, AllocAndSet) {
  auto n = alloc<int>();
  *n = 42;
  ASSERT_EQ(*n, 42);
  release(n);
}




TEST_F(HandlePtrTest, Structs) {
  struct Foo {
    int a;
    int b;
  };
  auto n = alloc<Foo>();
  n->a = 1;
  n->b = 2;
  ASSERT_EQ(n->a, 1);
  ASSERT_EQ(n->b, 2);
  release(n);
}
