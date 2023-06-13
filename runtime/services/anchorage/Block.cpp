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

#include <anchorage/Block.hpp>
#include <anchorage/Chunk.hpp>
#include <limits.h>
#include <string.h>

#include <anchorage/crc32.h>



uint32_t anchorage::Block::crc(void) {
  return xcrc32((const unsigned char *)data(), size(), 0xffffffff);
}




void anchorage::Block::dump(bool verbose, bool highlight) {
  if (verbose) {
    printf("%p sz:%5zu ", this, size());
    printf(" fl:%08x", m_flags);
    if (is_used()) {
      printf(" h:%8lx", (uint64_t)handle());
    } else {
      printf(" h:%8lx", (uint64_t)handle());
      printf(" lu:%10lu", -1);
      printf(" lk:%10lx", -1);
    }
    // return;
  }


  int color = 0;
  char c = '#';

  if (is_used()) {
    auto &handle = *this->handle();
    if (0) {       // (handle.anchorage.flags & ANCHORAGE_FLAG_LAZY_FREE) {
      color = 35;  // purple
      c = '#';
    } else if (is_locked()) {  // (handle.anchorage.locks > 0) {
      color = 31;              // red
      c = 'X';
    } else {
      color = 32;  // green
      c = '#';
    }

    if (handle.ptr != data()) {
      color = 33;  // yellow
      c = '?';
    }
  } else {
    color = 90;  // gray
    c = '-';
  }
  (void)c;

  if (highlight) {
    // Print a background color
    printf("\e[%dm", 100);
  }
  // color += 10;

  printf("\e[%dm", color);
  ssize_t sz = size();

  if (verbose) {
    // putchar('|');
    auto *d = (uint64_t *)this; //data();
    size_t count = (sz + 16) / 8;
    if (count > 8) count = 8;

    for (size_t i = 0; i < count; i++) {
      // if (i % 8 == 0) printf(" ");
      printf("%016lx ", d[i]);
    }
  } else {
    // c = ' ';
    // putchar(c);
    putchar('|');

		size_t count =  ((sz - anchorage::block_size) / anchorage::block_size);
		if (count > 64) count = 64;
    for (size_t i = 0; i <= count; i++)
      putchar(c);
  }
  printf("\e[0m");

  if (verbose) printf("\n");
}
