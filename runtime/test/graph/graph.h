// Copyright (c) 2015, The Regents of the University of California (Regents)
// See LICENSE.txt for license details

#ifndef GRAPH_H_
#define GRAPH_H_

#include <algorithm>
#include <cinttypes>
#include <cstddef>
#include <iostream>
#include <type_traits>
#include <unordered_set>
#include <unordered_map>

#include "pvector.h"
#include "util.h"
#include "platform_atomics.h"

#include <alaska/sim/handle_ptr.hpp>


// Syntactic sugar for an edge
template <typename SrcT, typename DstT = SrcT>
struct EdgePair {
  SrcT u;
  DstT v;

  EdgePair() {}

  EdgePair(SrcT u, DstT v)
      : u(u)
      , v(v) {}

  bool operator<(const EdgePair& rhs) const { return u == rhs.u ? v < rhs.v : u < rhs.u; }

  bool operator==(const EdgePair& rhs) const { return (u == rhs.u) && (v == rhs.v); }
};


// This is a super dumb and super simple graph implementation designed to *just* use
// alaska::sim::handle_ptrs internally to drive the HTLB.
template <typename NodeID_>
class Graph {
 public:
  class Node;

  using NodeHandle = alaska::sim::handle_ptr<Node>;
  using Edge = EdgePair<NodeID_, NodeID_>;
  using EdgeList = pvector<Edge>;
  using NodeHandleSet = std::unordered_set<NodeHandle>;


  class Node {
   public:
    NodeID_ id;
    // NOT A GOOD REPRESENTATION BUT I DONT CARE!
    NodeHandleSet out_neigh;
    NodeHandleSet in_neigh;

    Node(NodeID_ id)
        : id(id) {}
  };



  class Neighborhood {
    NodeHandleSet& neigh_;
    int start_offset;

   public:
    Neighborhood(NodeHandleSet& neigh, int start_offset = 0)
        : neigh_(neigh)
        , start_offset(start_offset) {}

    struct iterator {
      NodeHandleSet::iterator it;

      iterator(NodeHandleSet::iterator it)
          : it(it) {}

      NodeID_ operator*() { return (*it)->id; }
      void operator++() { ++it; }
      bool operator!=(const iterator& rhs) { return it != rhs.it; }
    };

    iterator begin() {
      auto it = neigh_.begin();
      for (int i = 0; i < start_offset; i++) {
        ++it;
      }
      return it;
    }
    iterator end() { return neigh_.end(); }
  };




  Graph(EdgeList& el) {
    for (auto& edge : el) {
      NodeID_ u = edge.u;
      NodeID_ v = edge.v;

      // don't add self edges.
      if (u == v) continue;

      num_edges_++;

      // printf("node %lu -> %lu (%p -> %p)\n", u, v, getNode(u).get(), getNode(v).get());

      getNode(u)->out_neigh.insert(getNode(v));
      getNode(v)->in_neigh.insert(getNode(u));
    }
  }


  int64_t num_nodes() { return nodes_.size(); }
  int64_t num_edges() { return num_edges_; }

  Range<NodeID_> vertices() { return Range<NodeID_>(num_nodes()); }



  bool directed() { return false; }
  Neighborhood out_neigh(NodeID_ n, int start_offset = 0) {
    return Neighborhood(getNode(n)->out_neigh, start_offset);
  }
  Neighborhood in_neigh(NodeID_ n, int start_offset = 0) {
    return Neighborhood(getNode(n)->in_neigh, start_offset);
  }

  int64_t out_degree(NodeID_ n) { return getNode(n)->out_neigh.size(); }
  int64_t in_degree(NodeID_ n) { return getNode(n)->in_neigh.size(); }

  void dump(void) {
    std::cout << "digraph {\n";
    for (auto& kv : nodes_) {
      NodeHandle node = kv.second;
      for (auto& neigh : node->out_neigh) {
        printf("  %lu -> %lu\n", node->id, neigh->id);
      }
    }
    std::cout << "}\n";
  }

 private:
  NodeHandle getNode(NodeID_ id) {
    auto it = nodes_.find(id);
    if (it == nodes_.end()) {
      nodes_[id] = alaska::sim::alloc<Node>(id);
      return nodes_[id];
    }
    return it->second;
  }

  int64_t num_edges_ = 0;
  std::unordered_map<NodeID_, NodeHandle> nodes_;
};


#endif  // GRAPH_H_
