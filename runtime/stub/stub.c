#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <alaska.h>

#define SS (sizeof(size_t))
#define ALIGN (sizeof(size_t))
#define ONES ((size_t)-1 / UCHAR_MAX)
#define HIGHS (ONES * (UCHAR_MAX / 2 + 1))
#define HASZERO(x) ((x)-ONES & ~(x)&HIGHS)

// Most of these functions are straight up stolen from musl libc
void setbuf(FILE *stream, char *buf) {
  // NOP
}

char *strstr(const char *hs, const char *ne) {
  size_t i;
  int c = ne[0];

  if (c == 0) return (char *)hs;

  for (; hs[0] != '\0'; hs++) {
    if (hs[0] != c) continue;
    for (i = 1; ne[i] != 0; i++)
      if (hs[i] != ne[i]) break;
    if (ne[i] == '\0') return (char *)hs;
  }

  return NULL;
}

char *__strchrnul(const char *s, int c) {
  c = (unsigned char)c;
  if (!c) return (char *)s + strlen(s);
  for (; *s && *(unsigned char *)s != c; s++)
    ;
  return (char *)s;
}


#define BITOP(a, b, op) \
  ((a)[(size_t)(b) / (8 * sizeof *(a))] op(size_t) 1 << ((size_t)(b) % (8 * sizeof *(a))))

size_t strcspn(const char *s, const char *c) {
  const char *a = s;
  size_t byteset[32 / sizeof(size_t)];

  if (!c[0] || !c[1]) return __strchrnul(s, *c) - a;

  memset(byteset, 0, sizeof byteset);
  for (; *c && BITOP(byteset, *(unsigned char *)c, |=); c++)
    ;
  for (; *s && !BITOP(byteset, *(unsigned char *)s, &); s++)
    ;
  return s - a;
}

char *strpbrk(const char *s, const char *b) {
  s += strcspn(s, b);
  return *s ? (char *)s : 0;
}

size_t __strlen_avx2(const char *s) {
  return strlen(s);
}

size_t strlen(const char *s) {
  const char *a = s;
#ifdef __GNUC__
  typedef size_t __attribute__((__may_alias__)) word;
  const word *w;
  for (; (uintptr_t)s % ALIGN; s++)
    if (!*s) return s - a;
  for (w = (const void *)s; !HASZERO(*w); w++)
    ;
  s = (const void *)w;
#endif
  for (; *s; s++)
    ;
  return s - a;
}




char *strchr(const char *s, int c) {
  // printf("strchr %p\n", s);
  char *r = __strchrnul(s, c);
  return *(unsigned char *)r == (unsigned char)c ? r : 0;
}


void *__memrchr(const void *m, int c, size_t n) {
  const unsigned char *s = m;
  c = (unsigned char)c;
  while (n--)
    if (s[n] == c) return (void *)(s + n);
  return 0;
}

char *strrchr(const char *s, int c) {
  return __memrchr(s, c, strlen(s) + 1);
}

void *memchr(const void *src, int c, size_t n) {
  const unsigned char *s = src;
  c = (unsigned char)c;
#ifdef __GNUC__
  for (; ((uintptr_t)s & ALIGN) && n && *s != c; s++, n--)
    ;
  if (n && *s != c) {
    typedef size_t __attribute__((__may_alias__)) word;
    const word *w;
    size_t k = ONES * c;
    for (w = (const void *)s; n >= SS && !HASZERO(*w ^ k); w++, n -= SS)
      ;
    s = (const void *)w;
  }
#endif
  for (; n && *s != c; s++, n--)
    ;
  return n ? (void *)s : 0;
}


int strcasecmp(const char *_l, const char *_r) {
  const unsigned char *l = (void *)_l, *r = (void *)_r;
  for (; *l && *r && (*l == *r || tolower(*l) == tolower(*r)); l++, r++)
    ;
  return tolower(*l) - tolower(*r);
}

int __strcasecmp_l(const char *l, const char *r, locale_t loc) {
  return strcasecmp(l, r);
}


char *strdup(const char *s) {
  size_t l = strlen(s);
  char *d = malloc(l + 1);
  if (!d) return NULL;
  return memcpy(d, s, l + 1);
}




char *__stpcpy(char *restrict d, const char *restrict s) {
#ifdef __GNUC__
  typedef size_t __attribute__((__may_alias__)) word;
  word *wd;
  const word *ws;
  if ((uintptr_t)s % ALIGN == (uintptr_t)d % ALIGN) {
    for (; (uintptr_t)s % ALIGN; s++, d++)
      if (!(*d = *s)) return d;
    wd = (void *)d;
    ws = (const void *)s;
    for (; !HASZERO(*ws); *wd++ = *ws++)
      ;
    d = (void *)wd;
    s = (const void *)ws;
  }
#endif
  for (; (*d = *s); s++, d++)
    ;

  return d;
}

char *strcpy(char *restrict dest, const char *restrict src) {
  __stpcpy(dest, src);
  return dest;
}


char *stpcpy(char *restrict dest, const char *restrict src) {
  __stpcpy(dest, src);
  return dest;
}

char *__stpncpy(char *restrict d, const char *restrict s, size_t n) {
#ifdef __GNUC__
  typedef size_t __attribute__((__may_alias__)) word;
  word *wd;
  const word *ws;
  if (((uintptr_t)s & ALIGN) == ((uintptr_t)d & ALIGN)) {
    for (; ((uintptr_t)s & ALIGN) && n && (*d = *s); n--, s++, d++)
      ;
    if (!n || !*s) goto tail;
    wd = (void *)d;
    ws = (const void *)s;
    for (; n >= sizeof(size_t) && !HASZERO(*ws); n -= sizeof(size_t), ws++, wd++)
      *wd = *ws;
    d = (void *)wd;
    s = (const void *)ws;
  }
#endif
  for (; n && (*d = *s); n--, s++, d++)
    ;
tail:
  memset(d, 0, n);
  return d;
}

char *strncpy(char *restrict d, const char *restrict s, size_t n) {
  __stpncpy(d, s, n);
  return d;
}


char *strcat(char *restrict dest, const char *restrict src) {
  strcpy(dest + strlen(dest), src);
  return dest;
}


char *strncat(char *restrict d, const char *restrict s, size_t n) {
  char *a = d;
  d += strlen(d);
  while (n && *s)
    n--, *d++ = *s++;
  *d++ = 0;
  return a;
}


extern void alaska_barrier_signal_join(void);
// This function is the "signal" function to the runtime that gets patched
// into the code whenever a barrier needs to occur.
__attribute__((preserve_most)) void __alaska_signal(void) {
  alaska_barrier_signal_join();
}



extern int __LLVM_StackMaps __attribute__((weak));
extern char __alaska_text_start[];
extern char __alaska_text_end[];

static void __attribute__((constructor)) alaska_init(void) {
  struct alaska_blob_config cfg;
  cfg.code_start = (uintptr_t)__alaska_text_start;
  cfg.code_end = (uintptr_t)__alaska_text_end;
  cfg.stackmap = &__LLVM_StackMaps;
  alaska_blob_init(&cfg);

}
