/*
 * This file is part of the Alaska Handle-Based Memory Management System
 *
 * Copyright (c) 2024, Nick Wanninger <ncw@u.northwestern.edu>
 * Copyright (c) 2024, The Constellation Project
 * All rights reserved.
 *
 * This is free software.  You are permitted to use, redistribute,
 * and modify it as specified in the file "LICENSE".
 */
#pragma once


#include "./graph_lite.h"
#include <stack>

namespace alaska {


  using namespace graph_lite;

  template <typename NodeType = int, typename EdgeType = float>
  class DirectedGraph : public graph_lite::Graph<NodeType, void, EdgeType, EdgeDirection::DIRECTED,
                            MultiEdge::DISALLOWED, SelfLoop::ALLOWED, Map::UNORDERED_MAP,
                            Container::UNORDERED_SET, Logging::DISALLOWED> {
   public:
    template <typename T>
    struct iter_wrapper {
      T _begin, _end;

      iter_wrapper(T begin, T end)
          : _begin(begin)
          , _end(end) {}

      T begin() { return _begin; }
      T end() { return _end; }
    };

    auto outgoing(const NodeType &n) {
      auto [begin, end] = this->out_neighbors(n);
      return iter_wrapper<decltype(begin)>(begin, end);
    }

    auto incoming(const NodeType &n) {
      auto [begin, end] = this->in_neighbors(n);
      return iter_wrapper<decltype(begin)>(begin, end);
    }




    // Given a start node, produce a vector that is the depth first
    // traversal starting from that node.
    std::vector<NodeType> depth_first_order(NodeType start) {
      // procedure DFS_iterative(G, v) is
      // let S be a stack
      // S.push(v)
      // while S is not empty do
      //     v = S.pop()
      //     if v is not labeled as discovered then
      //         label v as discovered
      //         for all edges from v to w in G.adjacentEdges(v) do
      //             S.push(w)

      std::vector<NodeType> out;
      std::set<NodeType> discovered;
      std::stack<NodeType> s;
      s.push(start);
      while (not s.empty()) {
        auto v = s.top();
        s.pop();

        // If v was not labeled as discovered
        if (discovered.find(v) == discovered.end()) {
          // label v as discovered
          discovered.insert(v);
          // add it to the list
          out.push_back(v);

          for (auto &[w, _] : this->outgoing(v)) {
            s.push(w);
          }
        }
      }
      return out;
    }


    std::vector<NodeType> breadth_first_order(NodeType start) {
      std::vector<NodeType> out;
      std::set<NodeType> explored;
      std::queue<NodeType> Q;
      Q.push(start);
      explored.insert(start);

      while (not Q.empty()) {
        auto v = Q.front();
        Q.pop();
        out.push_back(v);

        for (auto &[w, _] : this->outgoing(v)) {
          if (explored.find(w) == explored.end()) {
            explored.insert(w);
            Q.push(w);
          }
        }
      }

      return out;
    }
  };

};  // namespace alaska
