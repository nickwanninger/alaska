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
#include <alaska.h>
#include <alaska/internal.h>
#include <new>

ALASKA_EXPORT void *operator new(size_t size) {
  // Just call halloc
  return halloc(size);
}

ALASKA_EXPORT void *operator new[](size_t sz) {
  // Just call halloc
  return halloc(sz);
}

ALASKA_EXPORT void *operator new(size_t sz, const std::nothrow_t &) throw() {
  // Just call halloc
  return halloc(sz);
}

ALASKA_EXPORT void *operator new[](size_t sz, const std::nothrow_t &) throw() {
  // Just call halloc
  return halloc(sz);
}

ALASKA_EXPORT void operator delete(void *ptr)
#if defined(_GLIBCXX_USE_NOEXCEPT)
    _GLIBCXX_USE_NOEXCEPT
#else
#if defined(__GNUC__)
    // clang + libcxx on linux
    _NOEXCEPT
#endif
#endif
{
  // Just call hfree
  hfree(ptr);
}


ALASKA_EXPORT void operator delete[](void *ptr)
#if defined(_GLIBCXX_USE_NOEXCEPT)
    _GLIBCXX_USE_NOEXCEPT
#else
#if defined(__GNUC__)
    // clang + libcxx on linux
    _NOEXCEPT
#endif
#endif
{
  // Just call hfree
  hfree(ptr);
}


ALASKA_EXPORT void operator delete(void *ptr, size_t sz) {
  // Just call hfree
  hfree(ptr);
}

ALASKA_EXPORT void operator delete[](void *ptr, size_t sz)
#if defined(__GNUC__)
    _GLIBCXX_USE_NOEXCEPT
#endif
{
  // Just call hfree
  hfree(ptr);
}
