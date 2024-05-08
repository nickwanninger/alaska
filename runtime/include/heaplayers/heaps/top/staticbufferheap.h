/* -*- C++ -*- */

/*

  Heap Layers: An Extensible Memory Allocation Infrastructure
  
  Copyright (C) 2000-2020 by Emery Berger
  http://www.emeryberger.com
  emery@cs.umass.edu
  
  Heap Layers is distributed under the terms of the Apache 2.0 license.

  You may obtain a copy of the License at
  http://www.apache.org/licenses/LICENSE-2.0

*/

#pragma once

#ifndef HL_STATICBUFFERHEAP_H
#define HL_STATICBUFFERHEAP_H

namespace HL {

/**
 * Heap that satisfies all requests out of static buffer.
 */
template <int BufferSize>
class StaticBufferHeap {
 public:
  StaticBufferHeap() {}

  enum { Alignment = alignof(std::max_align_t) };

  void *malloc(size_t sz) {
    auto oldAllocated = allocated();
    auto prevPtr = _bufPtr;
    if (sz == 0) {
      sz = Alignment;
    }
    sz = sz + (Alignment - (sz % Alignment));
    if (allocated() + sizeof(Header) + sz > BufferSize) {
      return nullptr;
    }
    _bufPtr += sz + sizeof(Header);
    new (prevPtr) Header(sz);
    auto ptr = (Header *)prevPtr + 1;
    assert(sz <= getSize(ptr));
    assert(isValid(ptr));
    assert(allocated() == oldAllocated + sz + sizeof(Header));
#if 0
    tprintf::tprintf("allocated @ sz = @\n",
		     sz,
		     (void *) ptr);
#endif
    return ptr;
  }

  void *memalign(size_t alignment, size_t sz) {
    if (sz == 0 || !isPowerOf2(alignment)) {
      return nullptr;
    }

    // Ensure we're not breaking this heap's 'Alignment'.
    alignment = std::max(alignment, (size_t)Alignment);

    // Note that if we're already aligned, we'll still skip 'alignment' bytes:
    // we can't skip 0 bytes as we need space for 'Header'
    uintptr_t skip = alignment - ((uintptr_t)_bufPtr % alignment);
    while (skip < sizeof(Header)) {
      skip += alignment;
    }
    skip -= sizeof(Header); // requires 'sizeof(Header) % Alignment == 0'

    assert((skip % Alignment) == 0);

    _bufPtr += skip;

    if (void* p = malloc(sz)) {
      return p;
    }

    _bufPtr -= skip;
    return nullptr;
  }

  void free(void *) {}

  size_t getSize(void *ptr) {
    if (isValid(ptr)) {
      auto sz = ((Header *)ptr - 1)->size;
      //      tprintf::tprintf("size of @ = @\n", ptr, sz);
      return sz;
    } else {
      return 0;
    }
  }

  bool isValid(void *ptr) {
    if ((uintptr_t)ptr >= (uintptr_t)_buf) {
      if ((uintptr_t)ptr < (uintptr_t)_buf + BufferSize) {
        return true;
      }
    }
    return false;
  }

  size_t allocated() const { return (uintptr_t)_bufPtr - (uintptr_t)_buf; }

 private:
  class Header {
   public:
    Header(size_t sz) : size(sz) {}
    alignas(Alignment) size_t size;
  };

  inline bool isPowerOf2(size_t n) {
    return n && !(n & (n-1));
  }

  alignas(Alignment) char _buf[BufferSize];
  char *_bufPtr{_buf};
};

} // namespace

#endif
