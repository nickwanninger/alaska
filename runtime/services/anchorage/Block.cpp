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




int anchorage::Block::coalesce_free(anchorage::Chunk &chunk) {
  // if the previous is free, ask it to coalesce instead, as we only handle
  // left-to-right coalescing.
  if (prev() && prev()->is_free()) {
    return prev()->coalesce_free(chunk);
  }

  int changes = 0;
  Block *succ = next();

  // printf("coa %p\n", this);
  // chunk.dump(this, "Free");

  while (succ && succ->handle() == NULL) {
    set_next(succ->next());
    succ = succ->next();
    changes += 1;
  }

  if (next() == chunk.tos || next() == NULL) {
    // move the top of stack down if we can.
    // There are two cases to handle here.

    if (is_used()) {
      // 1. The block before `tos` is an allocated block. In this case, we move
      //    the `tos` right to the end of the allocated block.
      // void *after = ((uint8_t *)data() + round_up(handle()->size, anchorage::block_size));
      auto after = next();  // TODO: might not work!
      chunk.tos = (Block *)after;
      set_next((Block *)after);
      chunk.tos->set_next(NULL);
      chunk.tos->mark_as_free(chunk);
      changes++;
    } else {
      // 2. The block before `tos` is a free block. This block should become `tos`
      set_next(NULL);
      chunk.tos = this;
      changes++;
    }
  }
  return changes;
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
    for (size_t i = 0; i <= ((sz - anchorage::block_size) / anchorage::block_size); i++)
      putchar(c);
  }
  printf("\e[0m");

  if (verbose) printf("\n");
}
