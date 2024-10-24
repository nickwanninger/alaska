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


#include <alaska/ThreadCache.hpp>


namespace yukon {

  constexpr int csr_htbase = 0xc2;
  constexpr int csr_htdump = 0xc3;
  constexpr int csr_htinval = 0xc4;

  // Set the handle table base register in hardware to enable handle translation
  void set_handle_table_base(void *base);
  // Trigger an HTLB dump
  void dump_htlb(alaska::ThreadCache *tc);


  void init(void);

  alaska::ThreadCache *get_tc(void);


}
