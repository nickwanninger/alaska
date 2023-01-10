#pragma once

// this header contains c++ templates to enable easier alaska usage in c++ programs

#include <stdio.h>
#include <alaska.h>
#include <memory>

namespace alaska {

  template <typename T, int object_class = 4>
  class allocator : public std::allocator<T> {
   public:
    typedef size_t size_type;
    typedef const T *const_pointer;
    using value_type = T;
    template <class U>
    struct rebind {
      using other = allocator<U, object_class>;
    };

    T *allocate(size_type n, const void *hint = 0) {
      // fprintf(stderr, "Alloc %lu bytes.  %d\n", n * sizeof(T), object_class);
      T *ptr = (T *)halloc(n * sizeof(T));
      alaska_classify(ptr, object_class);
      return ptr;
    }

    void deallocate(T *p, size_type n) {
      // fprintf(stderr, "Dealloc %lu bytes (%p).\n", n * sizeof(T), p);
      hfree(p);
      // return std::allocator<T>::deallocate(p, n);
    }

    allocator() throw() : std::allocator<T>() { fprintf(stderr, "Hello allocator!\n"); }
    allocator(const allocator &a) throw() : std::allocator<T>(a) {}
    template <class U>
    allocator(const allocator<U, object_class> &a) throw() : std::allocator<T>(a) {}
    ~allocator() throw() {}
  };

};  // namespace alaska
