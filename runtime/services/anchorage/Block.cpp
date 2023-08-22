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



static void set_fg(uint32_t color) {
  uint8_t r = color >> 16;
  uint8_t g = color >> 8;
  uint8_t b = color >> 0;
  printf("\e[38;2;%d;%d;%dm", r, g, b);
}

static void set_bg(uint32_t color) {
  uint8_t r = color >> 16;
  uint8_t g = color >> 8;
  uint8_t b = color >> 0;
  printf("\e[48;2;%d;%d;%dm", r, g, b);
}

static void clear_format(void) {
  printf("\e[0m");
}


void anchorage::Block::dump_content(const char *message) {
  printf("%20s ", message);

  size_t count = size() / sizeof(uint64_t);
  auto *d = static_cast<uint64_t *>(data());
  if (count > 8) count = 8;
  printf("(blk=%p, handle=%p) ", this, handle());

  for (size_t i = 0; i < count; i++) {
    // if (i % 8 == 0) printf(" ");
    printf("%016lx ", d[i]);
  }
  printf("\n");
}

void anchorage::Block::dump(bool verbose, bool highlight) {
  // Red by default
  // int primary = 0xCB5141;    // dark
  // int secondary = 0xE68F33;  // light
  int primary = 0x22'22'22;    // dark
  int secondary = 0x33'33'33;  // light


  if (is_used()) {
    // Green if it's allocated
    primary = 0x759C9C;    // dark
    secondary = 0xB8CCCC;  // light
  }

  if (is_locked()) {
    // red if it's locked.
    // primary = 0xCB5141;    // dark
    // secondary = 0xCE9189;  // light
    primary = 0xE68F33;    // dark
    secondary = 0xF2C159;  // light
  }


  char c;


  if (highlight) {
    set_fg(0xFF'FF'FF);
    set_bg(primary);
    putchar('*');
    set_bg(secondary);
    c = '-';
  } else {
    set_fg(0xFF'FF'FF);
    set_bg(primary);
    putchar(' ');
    set_fg(primary);
    set_bg(secondary);
    c = '-';
  }


  //  size_t count = (size() / anchorage::block_size);
  //  printf(" %zu ", size());
  // putchar(' ');
  if (0) {
    set_fg(0x88'88'88);
    size_t count = (size() + 16) / 8;
    auto *d = static_cast<uint64_t *>(data());
    if (count > 8) count = 8;

    for (size_t i = 0; i < count; i++) {
      // if (i % 8 == 0) printf(" ");
      printf("%016lx ", d[i]);
    }
  } else {
    size_t count = (size() / anchorage::block_size);
    // printf(" %zu ", count);
    for (size_t i = 0; i < count; i++) {
      putchar(c);
    }
  }
  clear_format();

  return;
  // int color = 0;
  // char c = ' ';
  // if (is_used()) {
  //   auto &handle = *this->handle();
  //   if (0) {       // (handle.anchorage.flags & ANCHORAGE_FLAG_LAZY_FREE) {
  //     color = 35;  // purple
  //     c = '#';
  //   } else if (is_locked()) {  // (handle.anchorage.locks > 0) {
  //     color = 31;              // red
  //     c = 'X';
  //   } else {
  //     color = 32;  // green
  //     c = '#';
  //   }
  //
  //   if (handle.ptr != data()) {
  //     color = 33;  // yellow
  //     c = '?';
  //   }
  // } else {
  //   color = 90;  // gray
  //   c = '-';
  // }
  // (void)c;
  //
  // if (highlight) {
  //   color = 97;  // highlighted blocks should be white (with a background)
  //   // Print a background color
  //   printf("\e[%dm", 100);
  // }
  // // color += 10;
  //
  // printf("\e[%dm", color);
  // ssize_t sz = size();
  //
  // if (verbose) {
  //   // putchar('|');
  //   auto *d = (uint64_t *)this;  // data();
  //   size_t count = (sz + 16) / 8;
  //   if (count > 8) count = 8;
  //
  //   for (size_t i = 0; i < count; i++) {
  //     // if (i % 8 == 0) printf(" ");
  //     printf("%016lx ", d[i]);
  //   }
  // } else {
  //   // c = ' ';
  //   // putchar(c);
  //   putchar('|');
  //
  //   size_t count = ((sz - anchorage::block_size) / anchorage::block_size);
  //   if (count > 64) count = 64;
  //   for (size_t i = 0; i <= count; i++)
  //     putchar(c);
  // }
  // printf("\e[0m");
  //
  // if (verbose) printf("\n");
}
