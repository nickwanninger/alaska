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
