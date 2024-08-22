#include <gtest/gtest.h>
#include <gtest/gtest.h>

#include <alaska.h>
#include <alaska/Logger.hpp>
#include <vector>
#include <alaska/Heap.hpp>
#include <alaska/Runtime.hpp>
#include "alaska/ThreadCache.hpp"


#include <alaska/sim/handle_ptr.hpp>
#include <alaska/sim/HTLB.hpp>


#define L1_WAYS 4
#define L1_SETS 24

#define L2_WAYS 16
#define L2_SETS 32
#define TOTAL_ENTRIES (L1_WAYS * L1_SETS + L2_WAYS * L2_SETS)


static unsigned int hash_int(int x) {
  // A simple hash function for integers
  x = ((x >> 16) ^ x) * 0x45d9f3b;
  x = ((x >> 16) ^ x) * 0x45d9f3b;
  x = (x >> 16) ^ x;
  return x;
}


template <typename K, typename V>
struct HandleTree {
  alaska::sim::handle_ptr<HandleTree<K, V>> left;
  alaska::sim::handle_ptr<HandleTree<K, V>> right;
  K key;
  V value;

  HandleTree(K key, V value)
      : key(key)
      , value(value) {}

  V find(K key) {
    if (key == this->key) {
      return value;
    } else if (key < this->key) {
      if (left == nullptr) return {};
      return left->find(key);
    } else {
      if (left == nullptr) return {};
      return right->find(key);
    }
  }

  // dump this tree with indentation
  void dump(int indent = 0) {
    for (int i = 0; i < indent; i++) {
      printf("  ");
    }
    printf("%d: %d\n", key, value);
    if (left != nullptr) {
      left->dump(indent + 1);
    }
    if (right != nullptr) {
      right->dump(indent + 1);
    }
  }
};




class HTLBSimTest : public ::testing::Test {
 public:
  void SetUp() override {
    tc = runtime.new_threadcache();
    dump_region = (uint64_t *)calloc(sizeof(uint64_t), TOTAL_ENTRIES);
    // printf("region is %d big\n", TOTAL_ENTRIES);
  }




  void TearDown() override {
    runtime.del_threadcache(tc);
    tc = nullptr;
    // htlb.dump_entries(dump_region);
    // for (int i = 0; i < TOTAL_ENTRIES; i++) {
    //   if (dump_region[i] != 0) {
    //     printf("%4d: %lx\n", i, dump_region[i]);
    //   }
    // }

    // free((void *)dump_region);
  }

  template <typename T, typename... Args>
  alaska::sim::handle_ptr<T> alloc(Args &&...args) {
    alaska::sim::handle_ptr<T> ptr = (T *)tc->halloc(sizeof(T), true);
    new (&*ptr) T(std::forward<Args>(args)...);
    return ptr;
  }

  template <typename T>
  void release(alaska::sim::handle_ptr<T> h) {
    tc->hfree(h);
  }


  alaska::sim::handle_ptr<HandleTree<int, int>> make_tree(int left, int right) {
    if (left >= right) {
      return nullptr;
    }

    int mid = (left + right) / 2;
    int value = mid;
    auto root = alloc<HandleTree<int, int>>(mid, value);
    root->left = make_tree(left, mid);
    root->right = make_tree(mid + 1, right);
    return root;
  }

  void free_tree(alaska::sim::handle_ptr<HandleTree<int, int>> root) {
    if (root.get() == nullptr) {
      return;
    }
    free_tree(root->left);
    free_tree(root->right);
    release(root);
  }

  alaska::ThreadCache *tc;
  alaska::Runtime runtime;

  alaska::sim::HTLB htlb = alaska::sim::HTLB(L1_WAYS, L1_SETS, L2_WAYS, L2_SETS);
  uint64_t *dump_region = nullptr;
};




TEST_F(HTLBSimTest, AllocAndSet) {
  auto n = alloc<int>();
  *n = 42;
  ASSERT_EQ(*n, 42);
  release(n);
}




TEST_F(HTLBSimTest, AllocAndSetTwo) {
  auto a = alloc<int>();
  *a = 1;

  auto b = alloc<int>();
  *b = 2;

  printf("%p %p\n", a.get(), b.get());

  // ASSERT_EQ(*n, 42);
  release(a);
  release(b);
}


// TEST_F(HTLBSimTest, AllocAndManyRandom) {
//   srand(0);

//   // Allocate an array of ints and set them to 0
//   std::vector<alaska::sim::handle_ptr<int>> handles;
//   for (int i = 0; i < 10000; i++) {
//     auto n = alloc<int>();
//     *n = 0;
//     handles.push_back(n);
//   }


//   // Iterate 1000 times, and set random entries to random values
//   for (int i = 0; i < 1000; i++) {
//     int idx = rand() % handles.size();
//     int val = rand();
//     *handles[idx] = val;
//   }

//   // release all the handles
//   for (auto h : handles) {
//     release(h);
//   }
// }


// Test allocation of a tree, search, and free
TEST_F(HTLBSimTest, TreeAllocSearchFree) {
  int size = 50000;
  auto root = make_tree(0, size);

  // lookup 100 random entries
  for (int i = 1; i < 50000; i++) {
    int key = rand() % size;
    printf("key is %d\n", key);
    int expected_value = key;
    int val = root->find(key);
    ASSERT_EQ(val, expected_value);
  }

  // root->dump();
  free_tree(root);
}