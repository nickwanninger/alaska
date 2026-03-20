#include <gtest/gtest.h>
#include <gtest/gtest.h>

#include <alaska.h>
#include <alaska/util/Logger.hpp>
#include <vector>
#include <alaska/heaps/Heap.hpp>
#include <alaska/core/Runtime.hpp>
#include <alaska/core/ThreadCache.hpp>
#include <unordered_set>

#include <sim/handle_ptr.hpp>
#include <sim/HTLB.hpp>



// #define L1_ENTS 4
// #define L1_SETS 4
// #define L1_WAYS (L1_ENTS / L1_SETS)

// #define L2_WAYS 1
// #define L2_SETS 6
// #define TOTAL_ENTRIES (L1_WAYS * L1_SETS + L2_WAYS * L2_SETS)


class HTLBSimTest : public ::testing::Test {
 public:
  alaska::ThreadCache *tc;
  alaska::Runtime runtime;

  alaska::sim::HTLB htlb = alaska::sim::HTLB();
  alaska::handle_id_t *dump_region = nullptr;

  void SetUp() override {
    htlb.thread_cache = tc = runtime.new_threadcache();
    dump_region =
        (alaska::handle_id_t *)calloc(sizeof(alaska::handle_id_t), htlb.htlb.total_entries());
  }


  void TearDown() override {
    runtime.del_threadcache(tc);
    htlb.thread_cache = tc = nullptr;
    ::free(dump_region);
  }

  // alaska::handle_id_t *dump(void) {
  //   htlb.dump_entries(dump_region);
  //   return dump_region;
  // }
};




TEST_F(HTLBSimTest, AllocAndSet) {
  auto n = alaska::sim::alloc<int>();
  *n = 42;
  ASSERT_EQ(*n, 42);
  alaska::sim::release(n);
}


TEST_F(HTLBSimTest, AllocAndSetTwo) {
  auto a = alaska::sim::alloc<int>();
  *a = 1;

  auto b = alaska::sim::alloc<int>();
  *b = 2;

  alaska::sim::release(a);
  alaska::sim::release(b);
}


////////////////////////////////////////////////////////////////////////////////



class HashTable {
 public:
  struct Entry {
    uint64_t key;
    int value;
    alaska::sim::handle_ptr<Entry> next;

    Entry(uint64_t k, int v)
        : key(k)
        , value(v)
        , next(nullptr) {}
  };

  explicit HashTable(size_t size)
      : buckets(size) {}

  void insert(uint64_t key, int value) {
    size_t index = key % buckets.size();
    auto new_entry = alaska::sim::alloc<Entry>(key, value);

    if (!buckets[index]) {
      buckets[index] = new_entry;
    } else {
      auto current = buckets[index];
      while (current->next) {
        current = current->next;
      }
      current->next = new_entry;
    }
  }

  std::optional<int> lookup(uint64_t key) const {
    size_t index = key % buckets.size();
    auto current = buckets[index];

    while (current) {
      if (current->key == key) {
        return current->value;
      }
      current = current->next;
    }
    return std::nullopt;
  }

 private:
  std::vector<alaska::sim::handle_ptr<Entry>> buckets;
};



template <typename K, typename V>
struct Node {
  alaska::sim::handle_ptr<Node> left, right;
  K key;
  V value;
  int height;

  Node(K k, V v)
      : key(k)
      , value(v)
      , height(1)
      , left(nullptr)
      , right(nullptr) {}
};

template <typename K, typename V>
struct TreeMap {
  using NodePtr = alaska::sim::handle_ptr<Node<K, V>>;
  NodePtr root;

  int height(NodePtr n) { return n ? n->height : 0; }

  int balance_factor(NodePtr n) { return n ? height(n->left) - height(n->right) : 0; }

  NodePtr rotate_right(NodePtr y) {
    NodePtr x = y->left;
    NodePtr T2 = x->right;

    x->right = y;
    y->left = T2;

    y->height = std::max(height(y->left), height(y->right)) + 1;
    x->height = std::max(height(x->left), height(x->right)) + 1;

    return x;
  }

  NodePtr rotate_left(NodePtr x) {
    NodePtr y = x->right;
    NodePtr T2 = y->left;

    y->left = x;
    x->right = T2;

    x->height = std::max(height(x->left), height(x->right)) + 1;
    y->height = std::max(height(y->left), height(y->right)) + 1;

    return y;
  }

  NodePtr insert(NodePtr node, K key, V value) {
    if (!node) {
      return alaska::sim::alloc<Node<K, V>>(key, value);
    }

    if (key < node->key) {
      node->left = insert(node->left, key, value);
    } else if (key > node->key) {
      node->right = insert(node->right, key, value);
    } else {
      node->value = value;
      return node;
    }

    node->height = 1 + std::max(height(node->left), height(node->right));

    int balance = balance_factor(node);

    if (balance > 1 && key < node->left->key) {
      return rotate_right(node);
    }

    if (balance < -1 && key > node->right->key) {
      return rotate_left(node);
    }

    if (balance > 1 && key > node->left->key) {
      node->left = rotate_left(node->left);
      return rotate_right(node);
    }

    if (balance < -1 && key < node->right->key) {
      node->right = rotate_right(node->right);
      return rotate_left(node);
    }

    return node;
  }

  void insert(K key, V value) { root = insert(root, key, value); }

  NodePtr lookup(K key) {
    auto cur = root;
    while (cur) {
      if (key < cur->key) {
        cur = cur->left;
      } else if (key > cur->key) {
        cur = cur->right;
      } else {
        return cur;
      }
    }
    return nullptr;
  }

  void dump_node(NodePtr n, int depth = 0) {
    if (!n) return;

    if (depth == 0) {
      printf("%d [label=\"ROOT %d\"];\n", n->key, n->key);
    }

    if (n->left) {
      printf("%d -> %d;\n", n->key, n->left->key);
      dump_node(n->left, depth + 1);
    }
    if (n->right) {
      printf("%d -> %d;\n", n->key, n->right->key);
      dump_node(n->right, depth + 1);
    }
  }

  void dump() {
    printf("digraph G {\n");
    dump_node(root);
    printf("}\n");
  }

  void gather(NodePtr n, std::vector<Node<K, V> *> &nodes) {
    if (!n) return;
    auto *r = n.translate_untracked();
    nodes.push_back(r);
    gather(r->left, nodes);
    gather(r->right, nodes);
  }

  uint64_t total_pages(void) {
    std::unordered_set<uintptr_t> pages;
    std::vector<Node<K, V> *> nodes;
    gather(root, nodes);
    for (auto n : nodes) {
      uintptr_t page = (uintptr_t)n >> 12;
      pages.insert(page);
    }
    return pages.size();
  }
};

static TreeMap<int, int> alloc_tree(int node_count) {
  TreeMap<int, int> t;
  for (int i = 0; i < node_count; i++) {
    t.insert(i, rand());
  }
  auto pages = t.total_pages();
  printf("total pages %zu\n", pages);
  // t.dump();
  return t;
}


template <typename K, typename V>
static void walk_tree(alaska::sim::handle_ptr<Node<K, V>> n) {
  if (!n) return;

  walk_tree(n->left);
  walk_tree(n->right);
}

TEST_F(HTLBSimTest, TreeWalk) {
  return;
  srand(0);
  int count = 50'000;
  auto tree = alloc_tree(count);
  (void)tree;
  uint64_t improved_count = 0;

  printf("walking\n");
  htlb.reset();


  FILE *f = fopen("page_uses_tree.csv", "w");
  fprintf(f, "page,count,trial\n");
  for (int i = 0; i < 2000; i++) {
    int iterations_till_dump = (rand() + 200) % 1000;
    for (int j = 0; j < iterations_till_dump; j++) {
      auto ind = rand() % count;
      auto n = tree.lookup(ind);
    }
    // run localization
    bool improved = htlb.localize();
    if (improved) {
      improved_count++;
    }

    // for (uint64_t h = 1; h < alaska::ThreadCache::hotness_hist_size; h++) {
    //   fprintf(f, "%lu,%lu,%d\n", h, htlb.thread_cache->hotness_hist[h], i);
    // }
  }
  fclose(f);
  printf("improved: %zu times\n", improved_count);


  // auto r = dump();
  // for (size_t i = 0; i < TOTAL_ENTRIES; i++) {
  //   printf("%zu ", (size_t)r[i]);
  // }
}


TEST_F(HTLBSimTest, TableWalk) {
  return;
  srand(0);
  HashTable table(1000);
  uint64_t improved_count = 0;
  int count = 500'000;

  for (int i = 0; i < count; i++) {
    table.insert(i, rand());
  }

  printf("walking\n");
  htlb.reset();


  FILE *f = fopen("hist_table.csv", "w");
  fprintf(f, "hotness,count,trial\n");
  for (int i = 0; i < 10000; i++) {
    int iterations_till_dump = (rand() + 200) % 1000;
    for (int j = 0; j < iterations_till_dump; j++) {
      auto ind = rand() % count;
      auto n = table.lookup(ind);
    }
    // run localization
    bool improved = htlb.localize();
    if (improved) {
      improved_count++;
    }
  }
  fclose(f);
  printf("improved: %zu times\n", improved_count);


  // auto r = dump();
  // for (size_t i = 0; i < TOTAL_ENTRIES; i++) {
  //   printf("%zu ", (size_t)r[i]);
  // }
}
