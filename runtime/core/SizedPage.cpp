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

#include <alaska/SizedPage.hpp>



namespace alaska {


  void *SizedPage::alloc(const alaska::Mapping &m, alaska::AlignedSize size) {
    // TODO:
    return nullptr;
  }


  bool SizedPage::release(alaska::Mapping &m, void *ptr) {
    // Don't free, yet
    return true;
  }
}  // namespace alaska
