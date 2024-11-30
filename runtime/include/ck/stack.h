#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <alaska/alaska.hpp>
#include "./template_lib.h"
#include <new>
#include <string.h>

// No clue if this requires the library be linked.
#include <ck/new.h>
#include <ck/vec.h>

namespace ck {

  // This is just a super simple wrapper around a vector
  template <typename T>
  class stack {
   private:
    ck::vec<T> backing;

   public:
    stack(size_t initialCapacity = 4) { backing.ensure_capacity(initialCapacity); }

    ~stack() {}
    void push(const T& value) { backing.push(value); }

    T pop() {
      if (backing.is_empty()) {
        abort();
      }
      auto ind = backing.size() - 1;
      T value = backing[ind];
      backing.remove(ind);
      return value;
    }

    T& peek() const {
      if (backing.is_empty()) {
        abort();
      }
      auto ind = backing.size() - 1;
      return backing[ind];
    }

    bool is_empty() const { return backing.is_empty(); }
    size_t size() const { return backing.size(); }
  };
}  // namespace ck
