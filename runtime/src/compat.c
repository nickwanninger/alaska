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
  return (void*)ptr;
}

int alaska_wrapped_puts(const char *s) { return puts(alaska_lock_for_escape(s)); }


int alaska_wrapped_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
  printf("sigaction=%d\n", signum);
  if (signum == SIGSEGV) {
    return -1;
  }
  return sigaction(signum, act, oldact);
}


WEAK void *memset(void *s, int c, size_t n) {
  uint8_t *p = (uint8_t *)alaska_lock(s);
  for (size_t i = 0; i < n; i++)
    p[i] = c;
  alaska_unlock(s);
  return s;
}


WEAK void *memcpy(void *vdst, const void *vsrc, size_t n) {
  uint8_t *dst = (uint8_t *)alaska_lock(vdst);
  uint8_t *src = (uint8_t *)alaska_lock((void*)vsrc);
  for (size_t i = 0; i < n; i++) {
    dst[i] = src[i];
  }
  alaska_unlock(vdst);
  alaska_unlock((void*)vsrc);
  return vdst;
}


WEAK size_t strlen(const char *vsrc) {
  uint8_t *src = (uint8_t *)alaska_lock((void*)vsrc);
  size_t s = 0;
  for (s = 0; src[s]; s++) {
  }
  alaska_unlock((void*)vsrc);
  return s;
}

WEAK char *strcpy(char *vdest, const char *vsrc) {
  uint8_t *dest = (uint8_t *)alaska_lock(vdest);
  uint8_t *src = (uint8_t *)alaska_lock((void*)vsrc);
  size_t i;

  for (i = 0; src[i] != '\0'; i++)
    dest[i] = src[i];

  alaska_unlock(vdest);
  alaska_unlock((void*)vsrc);
  return vdest;
}

WEAK char *strncpy(char *vdest, const char *vsrc, size_t n) {
  uint8_t *dest = (uint8_t *)alaska_lock(vdest);
  uint8_t *src = (uint8_t *)alaska_lock((void*)vsrc);
  size_t i;

  for (i = 0; i < n && src[i] != '\0'; i++)
    dest[i] = src[i];
  for (; i < n; i++)
    dest[i] = '\0';
  alaska_unlock(vdest);
  alaska_unlock((void*)vsrc);

  return vdest;
}
