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

#include <alaska/HeapPage.hpp>


namespace alaska {

  HeapPage::~HeapPage() {}

  HeapPage::HeapPage(void *backing_memory)
      : memory(backing_memory) {
    mag_list = LIST_HEAD_INIT(mag_list);
  }



  void atomic_block_push(Block **list, Block *block) {
    // TODO: NOT SURE ABOUT THE CONSISTENCY OPTIONS HERE
    do {
      block->next = *list;
    } while (!__atomic_compare_exchange_n(
        list, &block->next, block, 1, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED));
  }

}  // namespace alaska
