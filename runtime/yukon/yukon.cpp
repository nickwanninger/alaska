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



#include <alaska/alaska.hpp>
#include <alaska/utils.h>
#include <alaska/Runtime.hpp>

#include <assert.h>

#ifdef __riscv
#define write_csr(reg, val) \
  ({ asm volatile("csrw " #reg ", %0" ::"rK"((uint64_t)val) : "memory"); })
#else
#define write_csr(reg, val)
// #warning YUKON expects riscv
#endif

static void set_ht_addr(void *addr) { write_csr(0xc2, addr); }

static alaska::Runtime *the_runtime = NULL;

void __attribute__((constructor(102))) alaska_init(void) {
  the_runtime = new alaska::Runtime();

  void *handle_table_base = the_runtime->handle_table.get_base();
  printf("Handle table at %p\n", handle_table_base);
  set_ht_addr(handle_table_base);

  auto tc = the_runtime->new_threadcache();
  printf("tc at %p\n", tc);

  size_t size = 1024;
  uint8_t *h = (uint8_t *)tc->halloc(size);
  printf("h = %p\n", h);
  for (size_t i = 0; i < size; i++)
    h[i] = i;

  for (size_t i = 0; i < size; i++) {
    //
  }

  tc->hfree(h);

  set_ht_addr(NULL);
  the_runtime->del_threadcache(tc);
}

void __attribute__((destructor)) alaska_deinit(void) {
  if (the_runtime != NULL) {
    delete the_runtime;
  }
}
