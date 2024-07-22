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

#pragma once


#ifdef ALASKA_TRACK_VALGRIND
// valgrind tool

#define ALASKA_TRACK_ENABLED 1
#define ALASKA_TRACK_HEAP_DESTROY 1  // track free of individual blocks on heap_destroy
#define ALASKA_TRACK_TOOL "valgrind"

#include <valgrind/valgrind.h>
#include <valgrind/memcheck.h>

#define alaska_track_malloc_size(p, reqsize, size, zero) \
  VALGRIND_MALLOCLIKE_BLOCK(p, size, 0 /*red zone*/, zero)
#define alaska_track_free(p, _size) VALGRIND_FREELIKE_BLOCK(p, 0 /*red zone*/)
#define alaska_track_resize(p, oldsize, newsize) \
  VALGRIND_RESIZEINPLACE_BLOCK(p, oldsize, newsize, 0 /*red zone*/)
#define alaska_track_mem_defined(p, size) VALGRIND_MAKE_MEM_DEFINED(p, size)
#define alaska_track_mem_undefined(p, size) VALGRIND_MAKE_MEM_UNDEFINED(p, size)
#define alaska_track_mem_noaccess(p, size) VALGRIND_MAKE_MEM_NOACCESS(p, size)

#endif



#ifndef alaska_track_resize
#define alaska_track_resize(p, oldsize, newsize) \
  alaska_track_free(p, oldsize);            \
  alaska_track_malloc(p, newsize, false)
#endif



#ifndef alaska_track_malloc_size
#define alaska_track_malloc_size(p, reqsize, size, zero)
#endif

#ifndef alaska_track_free
#define alaska_track_free(p, _size)
#endif


#ifndef alaska_track_align
#define alaska_track_align(p, alignedp, offset, size) alaska_track_mem_noaccess(p, offset)
#endif

#ifndef alaska_track_init
#define alaska_track_init()
#endif

#ifndef alaska_track_mem_defined
#define alaska_track_mem_defined(p, size)
#endif

#ifndef alaska_track_mem_undefined
#define alaska_track_mem_undefined(p, size)
#endif

#ifndef alaska_track_mem_noaccess
#define alaska_track_mem_noaccess(p, size)
#endif
