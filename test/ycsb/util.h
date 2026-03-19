#pragma once
#include <stdio.h>
#define LONG_STR_SIZE 21
static inline int ll2string(char *s, size_t len, long long v) {
    return snprintf(s, len, "%lld", v);
}
static inline int ull2string(char *s, size_t len, unsigned long long v) {
    return snprintf(s, len, "%llu", v);
}
