

// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil -*-
// Copyright 2019 The Mesh Authors. All rights reserved.
// Use of this source code is governed by the Apache License,
// Version 2.0, that can be found in the LICENSE file.

#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <fcntl.h>
#include <alaska.h>

void print_rss() { printf("\trss:\t%10.3f MB\n", alaska_get_rss_kb() / 1024.0); }

// use as (voidptr)ptr -- disgusting, but useful to minimize the clutter
// around using volatile.
#define voidptr void *)(intptr_t
#define NOINLINE __attribute__((noinline))
#define MB (1024 * 1024)
#define GB (1024 * 1024 * 1024)

void touch(volatile void *x) {
  if (x != NULL) *(volatile char *)x = 'a';
}

void *NOINLINE bench_alloc(size_t n) {
  volatile void *ptr = malloc(n);
  touch(ptr);

  return (voidptr)ptr;
}

void NOINLINE bench_free(void *ptr) {
  touch(ptr);
  free(ptr);
}

// Fig 4. "A Basic Strategy" from http://www.ece.iastate.edu/~morris/papers/10/ieeetc10.pdf
void NOINLINE basic_fragment(int64_t n, size_t m_total) {
  int64_t ci = 1;
  int64_t m_avail = m_total;
  size_t m_usage = 0;  // S_t in the paper
  size_t retained_off = 0;

  const size_t ptr_table_len = 2 * m_total / (ci * n);
  // fprintf(stdout, "ptr_table_len: %zu\n", ptr_table_len);
  volatile char *volatile *ptr_table =
      reinterpret_cast<volatile char *volatile *>(bench_alloc(ptr_table_len * sizeof(char *)));
  volatile char *volatile *retained_table =
      reinterpret_cast<volatile char *volatile *>(bench_alloc(ptr_table_len * sizeof(char *)));
  memset((voidptr)retained_table, 0, ptr_table_len * sizeof(char *));

  alaska_classify((void *)ptr_table, ALASKA_CLASS_VECTOR);
  alaska_classify((void *)ptr_table, ALASKA_CLASS_VECTOR);

  // show how much RSS we just burned through for the table of
  // pointers we just allocated
  print_rss();

  for (int64_t i = 1; m_avail >= 2 * ci * n; i++) {
    ci *= 2;
    // number of allocation pairs in this iteration
    const size_t pi = m_avail / (2 * ci * n);

    fprintf(stderr, "i:%4lld, ci:%5lld, n:%5lld, pi:%7zu (osize:%5lld)\n", (long long int)i, (long long int)ci,
        (long long int)n, pi, (long long int)(ci * n));

    // allocate two objects
    for (size_t k = 0; k < pi; k++) {
      ptr_table[2 * k + 0] = reinterpret_cast<volatile char *>(bench_alloc(ci * n));
      ptr_table[2 * k + 1] = reinterpret_cast<volatile char *>(bench_alloc(ci * n));
    }

    m_usage += ci * n * pi;

    // now free every other object
    for (size_t k = 0; k < pi; k++) {
      retained_table[retained_off++] = ptr_table[2 * k + 0];
      bench_free((voidptr)ptr_table[2 * k + 1]);
    }

    m_avail -= ci * n * pi;
  }

  fprintf(stderr, "allocated (and not freed) %.2f MB\n", ((double)m_usage) / MB);

  print_rss();

  alaska_barrier();
  print_rss();
  for (size_t i = 0; i < ptr_table_len; i++) {
    // touch((voidptr)retained_table[i]);
    bench_free((voidptr)retained_table[i]);
  }


  bench_free((voidptr)ptr_table);
  bench_free((voidptr)retained_table);
}

int main(int argc, char *argv[]) {
  print_rss();

  basic_fragment(512, 128 * MB);

  print_rss();

  return 0;
}
