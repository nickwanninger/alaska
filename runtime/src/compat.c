#include <stdio.h>
#include <signal.h>
#include <alaska.h>
#include <alaska/internal.h>

#define WEAK __attribute__((weak))

// This function exists to lock on extern escapes.
// TODO: determine if we should lock it forever or not...
static uint64_t escape_locks = 0;
static void *alaska_lock_for_escape(const void *ptr) {
  handle_t h;
  h.ptr = (void *)ptr;
  if (likely(h.flag != 0)) {
    atomic_inc(escape_locks, 1);
    // escape_locks++;
    // its easier to do this than to duplicate efforts and inline.
    void *t = alaska_lock((void *)ptr);
    alaska_unlock((void *)ptr);
    return t;
  }
  return (void *)ptr;
}

int alaska_wrapped_puts(const char *s) { return puts(alaska_lock_for_escape(s)); }


int alaska_wrapped_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
  printf("sigaction=%d\n", signum);
  if (signum == SIGSEGV) {
    return -1;
  }
  return sigaction(signum, act, oldact);
}



// DONE: memchr
// DONE: memcmp
// TODO: memcmpeq
// DONE: memcpy
// TODO: memmove
// TODO: mempcpy
// TODO: memrchr
// DONE: memset
// TODO: rawmemchr
// TODO: stpcpy
// TODO: stpncpy
// TODO: strcasecmp
// TODO: strcasecmp_l
// TODO: strcat
// TODO: strchr
// TODO: strchrnul
// DONE: strcmp
// DONE: strcpy
// DONE: strlen
// TODO: strncat
// TODO: strncmp
// DONE: strncpy
// TODO: strnlen
// TODO: strrchr
// TODO: wcschr
// TODO: wcscmp
// TODO: wcslen
// TODO: wcsncmp
// TODO: wcsnlen
// TODO: wcsrchr
// TODO: wmemchr
// TODO: wmemcmp
// TODO: wmemset



static void *__lock(void *ptr) {
	void *out = alaska_lock(ptr);
	alaska_unlock(ptr);
	return out;
}

#define LOCK(ptr) __lock((void*)(ptr))

int memcmp(const void *vl, const void *vr, size_t n) {
  const unsigned char *l = LOCK(vl), *r = LOCK(vr);
  for (; n && *l == *r; n--, l++, r++)
    ;
  return n ? *l - *r : 0;
}

int bcmp(const void *s1, const void *s2, size_t n) {
	return memcmp(s1, s2, n);
}


char *__stpcpy(char *restrict d, const char *restrict s) {
  for (; (*d = *s); s++, d++)
    ;

  return d;
}

int strcmp(const char *vl, const char *vr) {
  const char *l = alaska_lock((void *)vl);
  const char *r = alaska_lock((void *)vr);
  for (; *l == *r && *l; l++, r++)
    ;

  alaska_unlock((void *)vl);
  alaska_unlock((void *)vr);
  return *(unsigned char *)l - *(unsigned char *)r;
}

void *memchr(const void *mem, int ch, size_t n) {
  const unsigned char *c = alaska_lock((void *)mem);

  for (int i = 0; i < n; i++) {
    if (c[i] == (unsigned char)ch) {
      alaska_unlock((void *)mem);
      return (void *)((unsigned char *)mem + i);
    }
  }
  alaska_unlock((void *)mem);
  return NULL;
}

void *memset(void *s, int c, size_t n) {
  uint8_t *p = (uint8_t *)alaska_lock(s);
  for (size_t i = 0; i < n; i++)
    p[i] = c;
  alaska_unlock(s);
  return s;
}


void *memcpy(void *vdst, const void *vsrc, size_t n) {
  uint8_t *dst = (uint8_t *)alaska_lock(vdst);
  uint8_t *src = (uint8_t *)alaska_lock((void *)vsrc);
  for (size_t i = 0; i < n; i++) {
    dst[i] = src[i];
  }
  alaska_unlock(vdst);
  alaska_unlock((void *)vsrc);
  return vdst;
}


size_t strlen(const char *vsrc) {
  uint8_t *src = (uint8_t *)alaska_lock((void *)vsrc);
  size_t s = 0;
  for (s = 0; src[s]; s++) {
  }
  alaska_unlock((void *)vsrc);
  return s;
}

char *strcpy(char *vdest, const char *vsrc) {
  uint8_t *dest = (uint8_t *)alaska_lock(vdest);
  uint8_t *src = (uint8_t *)alaska_lock((void *)vsrc);
  size_t i;

  for (i = 0; src[i] != '\0'; i++)
    dest[i] = src[i];

  alaska_unlock(vdest);
  alaska_unlock((void *)vsrc);
  return vdest;
}

char *strncpy(char *vdest, const char *vsrc, size_t n) {
  uint8_t *dest = (uint8_t *)alaska_lock(vdest);
  uint8_t *src = (uint8_t *)alaska_lock((void *)vsrc);
  size_t i;

  for (i = 0; i < n && src[i] != '\0'; i++)
    dest[i] = src[i];
  for (; i < n; i++)
    dest[i] = '\0';
  alaska_unlock(vdest);
  alaska_unlock((void *)vsrc);

  return vdest;
}
