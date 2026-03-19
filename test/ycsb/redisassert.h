#pragma once
#include <stdio.h>
#include <stdlib.h>
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
static inline void _serverAssert(const char *e, const char *f, int l) {
    fprintf(stderr, "Assertion failed: %s (%s:%d)\n", e, f, l); abort();
}
static inline void _serverPanic(const char *f, int l, const char *msg, ...) {
    fprintf(stderr, "Panic at %s:%d: %s\n", f, l, msg); abort();
}
#define redis_unreachable() __builtin_unreachable()
#undef  assert
#define assert(e) (likely(e) ? (void)0 : (_serverAssert(#e, __FILE__, __LINE__), redis_unreachable()))
#define panic(...) (_serverPanic(__FILE__, __LINE__, __VA_ARGS__), redis_unreachable())
