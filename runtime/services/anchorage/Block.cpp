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

#include "./allocator.h"
#include <alaska/internal.h>



bool anchorage::Block::is_free(void) const {
  return handle() == NULL;
}


bool anchorage::Block::is_used(void) const {
  return !is_free();
}


auto anchorage::Block::handle(void) const -> alaska::Mapping * {
  if (m_handle == -1) return NULL;
  // cast the 32bit "pointer" into the real pointer
  return (alaska::Mapping *)(uint64_t)m_handle;
}
void anchorage::Block::set_handle(alaska::Mapping *handle) {
  if (handle == NULL) m_handle = -1;
  // Assign the packed 32bit "pointer"
  m_handle = (uint32_t)(uint64_t)handle;
}


auto anchorage::Block::next(void) const -> anchorage::Block * {
  if (m_next_off == 0) return nullptr;
  return const_cast<anchorage::Block *>(this + m_next_off);
}
void anchorage::Block::set_next(anchorage::Block *new_next) {
  if (new_next == nullptr) {
    m_next_off = 0;
  } else {
    m_next_off = new_next - this;
    new_next->set_prev(this);
  }
}

auto anchorage::Block::prev(void) const -> anchorage::Block * {
  if (m_prev_off == 0) return nullptr;
  return const_cast<anchorage::Block *>(this - m_prev_off);
}
void anchorage::Block::set_prev(anchorage::Block *new_prev) {
  if (new_prev == nullptr) {
    m_prev_off = 0;
  } else {
    m_prev_off = this - new_prev;
  }
}

size_t anchorage::Block::size(void) const {
  if (this->next() == NULL) return 0;
  return (off_t)this->next() - (off_t)this->data();
}

void *anchorage::Block::data(void) const {
  return (void *)(((uint8_t *)this) + sizeof(Block));
}

anchorage::Block *anchorage::Block::get(void *ptr) {
  uint8_t *bptr = (uint8_t *)ptr;
  return (anchorage::Block *)(bptr - sizeof(anchorage::Block));
}


int anchorage::Block::coalesce_free(anchorage::Chunk &chunk) {
  // if the previous is free, ask it to coalesce `this`
  if (prev() && prev()->is_free()) {
    return prev()->coalesce_free(chunk);
  }

  int changes = 0;
  Block *succ = next();
  // printf("coa %p ", block);
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
      void *after = ((uint8_t *)data() + round_up(handle()->size, anchorage::block_size));
      chunk.tos = (Block *)after;
      set_next((Block *)after);
      chunk.tos->set_next(NULL);
      chunk.tos->set_handle(NULL);
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
  int color = 0;
  char c = '#';

  if (is_used()) {
    auto &handle = *this->handle();
    if (handle.anchorage.flags & ANCHORAGE_FLAG_LAZY_FREE) {
      color = 35;  // purple
      c = '_';
    } else if (handle.anchorage.locks > 0) {
      color = 31;  // red
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
    color = 34;
  }
  printf("\e[%dm", color);
  ssize_t sz = size();

  // for (int i = 0; i < 16; i++)putchar('|');
  //  for (int i = 0; i <= sz; i++) putchar(c);

  putchar('|');
  for (int i = 0; i <= ((sz - anchorage::block_size) / anchorage::block_size); i++)
    putchar(c);
  printf("\e[0m");

  if (verbose) {
    printf(" (%zu)", sz);
  }
}
