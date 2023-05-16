/*
 * This file is part of the Alaska Handle-Based Memory Management System
 *
 * Copyright (c) 2023, Nick Wanninger <ncw@u.northwestern.edu>
 * Copyright (c) 2023, The Constellation Project
 * All rights reserved.
 *
 * This is free software.  You are permitted to use, redistribute,
 * and modify it as specified in the file "LICENSE".
 */
#pragma once


namespace alaska {
  // building c++ iterators is frustrating, so this abstracts it
  template <typename T>
  class iterator {
   public:
    using reference = T &;
    using pointer = T *;

    friend bool operator==(const iterator &a, const iterator &b) {
      return a.get() == b.get();
    };
    friend bool operator!=(const iterator &a, const iterator &b) {
      return a.get() != b.get();
    };

    auto operator*() const -> reference {
      return *get();
    }
    auto operator->() -> pointer {
      return get();
    }


    auto operator++() -> iterator<T> & {
      step();
      return *this;
    }

    auto operator++(int) -> iterator<T> {
      iterator tmp = *this;
      ++(*this);
      return tmp;
    }

    // You need to implement this.
    virtual void step() = 0;
    virtual T *get() const = 0;
  };
}  // namespace alaska