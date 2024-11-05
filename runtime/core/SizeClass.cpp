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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <alaska/SizeClass.hpp>
#include <alaska/HeapPage.hpp>
#include <cstdint>
#include <alaska/utils.h>
#include <math.h>


#define MI_MAX_ALIGN_SIZE 16  // sizeof(max_align_t)


// Much of the sizeclass logic here is taken from mimalloc.
#define MI_INTPTR_SHIFT (4)
#define MI_SIZE_SHIFT (3)

#define MI_INTPTR_SIZE (1 << MI_INTPTR_SHIFT)
#define MI_INTPTR_BITS (MI_INTPTR_SIZE * 8)

#define MI_ALIGN4W


#if (SIZE_MAX / 2) > LONG_MAX
#define MI_ZU(x) x##ULL
#define MI_ZI(x) x##LL
#else
#define MI_ZU(x) x##UL
#define MI_ZI(x) x##L
#endif


#define MI_SIZE_SIZE (1 << MI_SIZE_SHIFT)
#define MI_SIZE_BITS (MI_SIZE_SIZE * 8)

#define MI_LARGE_PAGE_SIZE (MI_ZU(1) << MI_LARGE_PAGE_SHIFT)
#define MI_LARGE_OBJ_SIZE_MAX (alaska::page_size / 2)  // 2MiB
#define MI_LARGE_OBJ_WSIZE_MAX (MI_LARGE_OBJ_SIZE_MAX / sizeof(uintptr_t))



static inline size_t mi_ctz32(uint32_t x) {
  // de Bruijn multiplication, see
  // <http://supertech.csail.mit.edu/papers/debruijn.pdf>
  static const unsigned char debruijn[32] = {0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4,
      8, 31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9};
  if (x == 0) return 32;
  return debruijn[((x & -(int32_t)x) * 0x077CB531UL) >> 27];
}



static inline size_t mi_clz32(uint32_t x) {
  // de Bruijn multiplication, see
  // <http://supertech.csail.mit.edu/papers/debruijn.pdf>
  static const uint8_t debruijn[32] = {31, 22, 30, 21, 18, 10, 29, 2, 20, 17, 15, 13, 9, 6, 28, 1,
      23, 19, 11, 3, 16, 14, 7, 24, 12, 4, 8, 25, 5, 26, 27, 0};
  if (x == 0) return 32;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  return debruijn[(uint32_t)(x * 0x07C4ACDDUL) >> 27];
}



static inline size_t mi_clz(uintptr_t x) {
  if (x == 0) return MI_INTPTR_BITS;
  size_t count = mi_clz32((uint32_t)(x >> 32));
  if (count < 32) return count;
  return (32 + mi_clz32((uint32_t)x));
}



// "bit scan reverse": Return index of the highest bit (or MI_INTPTR_BITS if `x`
// is zero)
static inline size_t mi_bsr(uintptr_t x) {
  return (x == 0 ? MI_INTPTR_BITS : MI_INTPTR_BITS - 1 - mi_clz(x));
}


#define WORD_SHIFT 4
#define WORD_SIZE (1 << WORD_SHIFT)
// Align a byte size to a size in _machine words_,
// i.e. byte size == `wsize*sizeof(void*)`.
static inline size_t _mi_wsize_from_size(size_t size) { return (size + WORD_SIZE - 1) / WORD_SIZE; }



// clang-format off
// Empty page queues for every cls
// #define SC(sz)  { NULL, NULL, (sz)*sizeof(uintptr_t) }
#define SC(sz)  (sz)*WORD_SIZE
#define SIZE_CLASSES \
  { SC(     1), SC(     2), SC(     3), SC(     4), SC(     5), SC(     6), SC(     7), SC(     8), /* 8 */ \
    SC(    10), SC(    12), SC(    14), SC(    16), SC(    20), SC(    24), SC(    28), SC(    32), /* 16 */ \
    SC(    40), SC(    48), SC(    56), SC(    64), SC(    80), SC(    96), SC(   112), SC(   128), /* 24 */ \
    SC(   160), SC(   192), SC(   224), SC(   256), SC(   320), SC(   384), SC(   448), SC(   512), /* 32 */ \
    SC(   640), SC(   768), SC(   896), SC(  1024), SC(  1280), SC(  1536), SC(  1792), SC(  2048), /* 40 */ \
    SC(  2560), SC(  3072), SC(  3584), SC(  4096), SC(  5120), SC(  6144), SC(  7168), SC(  8192), /* 48 */ \
    SC( 10240), SC( 12288), SC( 14336), SC( 16384), SC( 20480), SC( 24576), SC( 28672), SC( 32768), /* 56 */ \
    SC( 40960), SC( 49152), SC( 57344), SC( 65536), SC( 81920), SC( 98304), SC(114688), SC(131072), /* 64 */ \
    SC(163840), SC(196608), SC(229376), SC(262144), SC(327680), SC(393216), SC(458752), SC(524288), /* 72 */ \
    SC(0) /* Huge Class */}

// clang-format on


static size_t cls_sizes[] = SIZE_CLASSES;

namespace alaska {


  int size_to_class(size_t sz) {
    // if (sz >= alaska::huge_object_thresh) {
    //   return alaska::class_huge;
    // }
    size_t wsize = _mi_wsize_from_size(sz);
    if (wsize == 0) wsize = 1;
    uint8_t cls;
    if (wsize < 8) {
      cls = wsize - 1;
    } else {
      // if (wsize <= 16) {
      //   wsize = (wsize + 3) & ~3;
      // }  // round to 4x word sizes
      wsize--;
      // find the highest bit
      uint8_t b = (uint8_t)mi_bsr(wsize);  // note: wsize != 0
      // and use the top 3 bits to determine the bin (~12.5% worst internal fragmentation).
      // - adjust with 3 because we use do not round the first 8 sizes
      //   which each get an exact bin
      cls = ((b << 2) + (uint8_t)((wsize >> (b - 2)) & 0x03)) - 4;
    }
    return cls;
  }


  size_t class_to_size(int cls) { return cls_sizes[cls]; }


  size_t round_up_size(size_t sz) {
    // Simply ask for the size
    return class_to_size(size_to_class(sz));
  }

  bool should_be_huge_object(size_t size) {
    // int cls = size_to_class(size);

    // cls = 1;

    if (size > alaska::huge_object_thresh) {
      return true;
    }
    return false;
  }

}  // namespace alaska
