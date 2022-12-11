#include <stdio.h>
#include <signal.h>
#include <alaska.h>

extern void *alaska_lock_for_escape(const void *ptr);  // in runtime.c
extern void *alaska_lock(const void *ptr);             // in runtime.c
extern void alaska_unlock(const void *ptr);            // in runtime.c

int alaska_wrapped_puts(const char *s) { return puts(alaska_lock_for_escape(s)); }


int alaska_wrapped_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
	printf("sigaction=%d\n", signum);
  if (signum == SIGSEGV) {
    return -1;
  }
  return sigaction(signum, act, oldact);
}




void *memset(void *s, int c, size_t n) {
  uint8_t *p = (uint8_t *)alaska_lock(s);
  for (size_t i = 0; i < n; i++) {
    p[i] = c;
  }

  alaska_unlock(s);
  return s;
}


void *memcpy(void *vdst, const void *vsrc, size_t n) {
  uint8_t *dst = (uint8_t *)alaska_lock(vdst);
  uint8_t *src = (uint8_t *)alaska_lock(vsrc);
  for (size_t i = 0; i < n; i++) {
    dst[i] = src[i];
  }
  alaska_unlock(vdst);
  alaska_unlock(vsrc);
	return vdst;
}
