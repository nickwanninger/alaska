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

#include <anchorage/Anchorage.hpp>
#include <limits.h>
#include <stdint.h>


namespace anchorage {

  typedef uint64_t word_t;
  enum { BITS_PER_WORD = sizeof(word_t) * CHAR_BIT };
#define WORD_OFFSET(b) ((b) / BITS_PER_WORD)
#define BIT_OFFSET(b) ((b) % BITS_PER_WORD)

  // Just a bitmap class using 64 bit values. Construct
  // with the length you need. Cannot grow!
  class Bitmap {
   public:
    Bitmap(uint64_t length)
        : m_length(length) {
      // TODO: take in backing memory if we don't want to use the system malloc
      m_bits = (uint64_t *)calloc(length / sizeof(*m_bits), 1);
    }


    auto size(void) const {
      return m_length;
    }

    inline void set_all(void) {
      for (unsigned long i = 0; i < m_length / BITS_PER_WORD; i++) {
        m_bits[i] = -1;
      }
    }

    inline void clear_all(void) {
      for (unsigned long i = 0; i < m_length / BITS_PER_WORD; i++) {
        m_bits[i] = 0;
      }
    }


    bool get(int n) {
      word_t bit = m_bits[WORD_OFFSET(n)] & (1 << BIT_OFFSET(n));
      return bit != 0;
    }

    void set(int n) {
      m_bits[WORD_OFFSET(n)] |= (1 << BIT_OFFSET(n));
    }

    void clear(int n) {
      m_bits[WORD_OFFSET(n)] &= ~(1 << BIT_OFFSET(n));
    }

   protected:
   private:
    uint64_t m_length;  // Might as well make this 64 bits (alignment)
    uint64_t *m_bits = NULL;
  };
}  // namespace anchorage
