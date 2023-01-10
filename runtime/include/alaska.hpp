#pragma once

// this header contains c++ templates to enable easier alaska usage in c++ programs

#include <stdio.h>
#include <alaska.h>
#include <memory>

#include <vector>
#include <map>

namespace alaska {

  template <typename T, int object_class = 4>
  class allocator : public std::allocator<T> {
   public:
    using value_type = T;
    using reference = T &;
    using pointer = T *;

    template <class U>
    struct rebind {
      using other = allocator<U, object_class>;
    };

    T *allocate(size_t n, const void *hint = 0) {
      // fprintf(stderr, "Alloc %lu bytes.  %d\n", n * sizeof(T), object_class);
      T *ptr = (T *)halloc(n * sizeof(T));
      alaska_classify(ptr, object_class);
      return ptr;
    }

    void deallocate(T *p, size_t n) {
      // fprintf(stderr, "Dealloc %lu bytes (%zx).\n", n * sizeof(T), (uint64_t)p);
      hfree(p);
    }

    allocator() throw() : std::allocator<T>() {
      // fprintf(stderr, "Hello allocator!\n");
    }
    allocator(const allocator &a) throw() : std::allocator<T>(a) {}
    template <class U>
    allocator(const allocator<U, object_class> &a) throw() : std::allocator<T>(a) {}
    ~allocator() throw() {}
  };


  template <typename T>
  using vector = std::vector<T, alaska::allocator<T, ALASKA_CLASS_VECTOR>>;

  template <typename Key, typename Value>
  using map = std::map<Key, Value, std::less<Key>, alaska::allocator<std::pair<const Key, Value>, ALASKA_CLASS_MAP>>;



};  // namespace alaska
