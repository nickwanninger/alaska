// -*- C++ -*-

#ifndef HL_ONEHEAP_H
#define HL_ONEHEAP_H

#include "utility/singleton.h"

namespace HL {

  template <class TheHeap>
  class OneHeap : public singleton<TheHeap> {
  public:
    
    enum { Alignment = TheHeap::Alignment };
    
    static inline void * malloc (size_t sz) {
      return singleton<TheHeap>::getInstance().malloc (sz);
    }

    // 'auto' because some free() return bool, others void
    static inline auto free (void * ptr) {
      return singleton<TheHeap>::getInstance().free (ptr);
    }
    
    static inline size_t getSize (void * ptr) {
      return singleton<TheHeap>::getInstance().getSize (ptr);
    }

    static inline void * memalign (size_t alignment, size_t sz) {
      return singleton<TheHeap>::getInstance().memalign(alignment, sz);
    }
  };

}

#endif
