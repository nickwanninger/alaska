#include <stdlib.h>

extern "C" void __cxa_pure_virtual() { while (1); }

void *operator new(size_t size) {
  return malloc(size);
}

void *operator new[](size_t size) {
  return malloc(size);
}

void operator delete(void *ptr) noexcept {
  free(ptr);
}

void operator delete[](void *ptr) noexcept {
  free(ptr);
}

void operator delete(void *ptr, size_t s) noexcept {
  free(ptr);
}

void operator delete[](void *ptr, size_t s) noexcept {
  free(ptr);
}
