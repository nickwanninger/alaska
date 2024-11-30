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
#include <ck/option.h>


namespace ck {

  template <typename T>
  class queue {
   private:
    off_t front, back;
    size_t capacity;
    T* backing;

    off_t off(off_t o) { return o & (capacity - 1); }

    void grow(void) {
      auto new_cap = capacity * 2;
      T* new_backing = alaska::make_object_array<T>(new_cap);

      size_t count = 0;
      while (back != front)
        new_backing[count++] = backing[off(back++)];

      alaska::delete_object_array<T>(backing, capacity);

      backing = new_backing;
      capacity = new_cap;
      front = count;
      back = 0;
    }

   public:
    queue(size_t initial_cap_scale = 2) {
      capacity = 1 << initial_cap_scale;
      backing = alaska::make_object_array<T>(capacity);
      front = back = 0;
    }

    ~queue() { alaska::delete_object_array<T>(backing, capacity); }



    bool is_full(void) { return (size_t)(front - back) == capacity; }
    bool is_empty(void) { return front == back; }

    void push(const T& value) {
      if (is_full()) grow();
      backing[off(front++)] = value;
    }

    ck::opt<T> pop() {
      if (is_empty()) return None;
      return Some(backing[off(back++)]);
    }



    size_t size() const { return front - back; }
  };
}  // namespace ck
