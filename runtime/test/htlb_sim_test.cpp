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


#define L1_ENTS 96
#define L1_SETS 24
#define L1_WAYS (L1_ENTS / L1_SETS)

#define L2_WAYS 16
#define L2_SETS 32
#define TOTAL_ENTRIES (L1_WAYS * L1_SETS + L2_WAYS * L2_SETS)


class HTLBSimTest : public ::testing::Test {
 public:
  alaska::ThreadCache *tc;
  alaska::Runtime runtime;

  alaska::sim::HTLB htlb = alaska::sim::HTLB(L1_WAYS, L1_SETS, L2_WAYS, L2_SETS);
  uint64_t *dump_region = nullptr;

  void SetUp() override {
    htlb.thread_cache = tc = runtime.new_threadcache();
    dump_region = (uint64_t *)calloc(sizeof(uint64_t), TOTAL_ENTRIES);
  }


  void TearDown() override {
    runtime.del_threadcache(tc);
    htlb.thread_cache = tc = nullptr;
    ::free(dump_region);
  }

  uint64_t *dump(void) {
    htlb.dump_entries(dump_region);
    return dump_region;
  }
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

template <typename K, typename V>
struct Node {
  alaska::sim::handle_ptr<Node> left, right;
  int key;
  int value;
};


template <typename K, typename V>
struct TreeMap {
  using NodePtr = alaska::sim::handle_ptr<Node<K, V>>;
  NodePtr root;

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

  void insert(K key, V value) {
    // Allocate a new node, and insert it into the tree without balancing
    auto new_node = alaska::sim::alloc<Node<K, V>>();
    new_node->key = key;
    new_node->value = value;
    if (!root) {
      root = new_node;
      return;
    }

    NodePtr parent = nullptr;
    NodePtr cur = root;
    while (cur) {
      parent = cur;
      if (key < cur->key) {
        cur = cur->left;
      } else if (key > cur->key) {
        cur = cur->right;
      } else {
        // Key already exists, update the value
        cur->value = value;
        return;
      }
    }

    if (key < parent->key) {
      parent->left = new_node;
    } else {
      parent->right = new_node;
    }
  }
};

static TreeMap<int, int> alloc_tree(int node_count) {
  TreeMap<int, int> t;
  for (int i = 0; i < node_count; i++) {
    t.insert(rand(), rand());
  }
  return t;
}


template <typename K, typename V>
static void walk_tree(alaska::sim::handle_ptr<Node<K, V>> n) {
  if (!n) return;

  walk_tree(n->left);
  walk_tree(n->right);
}

TEST_F(HTLBSimTest, AllocAndSetThree) {
  srand(0);
  auto tree = alloc_tree(16);
  (void)tree;

  printf("walking\n");
  htlb.reset();
  walk_tree(tree.root);


  auto r = dump();

  int num_entries = 0;

  auto get_color_comp = [&num_entries](int ind) {
    return (uint8_t)(255 * ((float)ind / (float)num_entries));
  };

  auto trace = htlb.get_access_trace();
  num_entries = trace.size();

  for (int i = 1; i < trace.size(); i++) {
    auto last = trace[i - 1];
    auto cur = trace[i];
    printf("  x%lx -> x%lx[constraint=true, color=gray, label=%d]\n", last, cur, i);
  }



  num_entries = 0;
  for (int i = 0; i < TOTAL_ENTRIES; i++) {
    if (r[i] != 0) num_entries++;
  }

  uint64_t last = 0;
  int ind = 0;
  for (int i = TOTAL_ENTRIES - 1; i >= 0; i--) {
    // continue;
    uint64_t cur = r[i];

    if (cur == 0) {
      continue;
    }
    if (last == 0) {
      last = cur;
      continue;
    }

    int aind = ind++;

    printf("  x%lx [shape=rectangle, label=\"x%lx (%d)\"]\n", last, last, aind);

    // auto r = get_color_comp(aind);
    // auto b = 255 - r;
    // auto g = 0;
    // printf("  x%lx -> x%lx[constraint=false, color=\"#%02x%02x%02x\"]\n", last, cur, r, g, b);
    // printf("  x%lx [label=\"x%lx (%d)\"]\n", cur, cur, ind++);
    last = cur;
  }
  printf("  x%lx [shape=rectangle, label=\"x%lx (%d)\"]\n", last, last, ind);
}

////////////////////////////////////////////////////////////////////////////////

// #include "graph/generator.h"
// typedef float ScoreT;
// using NodeID = uint64_t;
// const float kDamp = 0.85;

// pvector<ScoreT> PageRankPullGS(
//     Graph<uint64_t> &g, int max_iters, double epsilon = 0, bool logging_enabled = false) {
//   const ScoreT init_score = 1.0f / g.num_nodes();
//   const ScoreT base_score = (1.0f - kDamp) / g.num_nodes();
//   pvector<ScoreT> scores(g.num_nodes(), init_score);
//   pvector<ScoreT> outgoing_contrib(g.num_nodes());
//   for (NodeID n = 0; n < g.num_nodes(); n++)
//     outgoing_contrib[n] = init_score / g.out_degree(n);
//   for (int iter = 0; iter < max_iters; iter++) {
//     double error = 0;
//     for (NodeID u = 0; u < g.num_nodes(); u++) {
//       ScoreT incoming_total = 0;
//       for (NodeID v : g.in_neigh(u))
//         incoming_total += outgoing_contrib[v];
//       ScoreT old_score = scores[u];
//       scores[u] = base_score + kDamp * incoming_total;
//       error += fabs(scores[u] - old_score);
//       outgoing_contrib[u] = scores[u] / g.out_degree(u);
//     }
//     // if (logging_enabled) PrintStep(iter, error);
//     if (error < epsilon) break;
//   }
//   return scores;
// }




// TEST_F(HTLBSimTest, GraphYay) {
//   Generator<NodeID> gen(4, 4);
//   auto el = gen.GenerateEL(true);

//   Graph<NodeID> g(el);
//   printf("Edge list: %zu edges\n", el.size());
//   printf("Graph has %zu nodes and %zu edges\n", g.num_nodes(), g.num_edges());

//   g.dump();

//   auto scores = PageRankPullGS(g, 100, 0.0001, true);
//   for (NodeID n = 0; n < g.num_nodes(); n++) {
//     printf("Node %lu: %f\n", n, scores[n]);
//   }
// }
