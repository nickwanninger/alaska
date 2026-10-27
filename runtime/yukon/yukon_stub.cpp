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

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <alaska/core/ThreadCache.hpp>
#include <alaska/core/Runtime.hpp>
#include <assert.h>
#include <sys/signal.h>


#include "./shared.h"



#define PUBLIC __attribute__((visibility("default")))

extern "C" PUBLIC void yukon_enable_localization(int enable) {}

// -------------------------------------------------------------- //
//                     Allocation Interface                       //
// -------------------------------------------------------------- //



PUBLIC void *operator new(size_t size) { return malloc(size); }
PUBLIC void *operator new[](size_t size) { return malloc(size); }
PUBLIC void operator delete(void *ptr) { free(ptr); }
PUBLIC void operator delete(void *ptr, unsigned long) { free(ptr); }
PUBLIC void operator delete[](void *ptr) { free(ptr); }


extern "C" {
PUBLIC void *malloc(size_t size) { return alaska::stub_malloc(size); }
PUBLIC void *calloc(size_t size, size_t count) { return alaska::stub_calloc(size, count); }
PUBLIC void *realloc(void *ptr, size_t newsize) { return alaska::stub_realloc(ptr, newsize); }
PUBLIC void free(void *ptr) { return alaska::stub_free(ptr); }
PUBLIC size_t malloc_usable_size(void *ptr) { return alaska::stub_malloc_usable_size(ptr); }
PUBLIC int posix_memalign(void **memptr, size_t align, size_t size) {
  return shared_posix_memalign(alaska::stub_malloc_aligned, memptr, align, size);
}
PUBLIC void *aligned_alloc(size_t align, size_t size) {
  return shared_aligned_alloc(alaska::stub_malloc_aligned, align, size);
}
PUBLIC void *memalign(size_t align, size_t size) {
  return shared_memalign(alaska::stub_malloc_aligned, align, size);
}
PUBLIC void *valloc(size_t size) { return shared_valloc(alaska::stub_malloc_aligned, size); }
}
