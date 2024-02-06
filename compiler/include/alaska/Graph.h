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
    // auto in(const NodeType &n) {
    //   return iter_wrapper(in_neighbors(n));
    // }
  };

};  // namespace alaska
