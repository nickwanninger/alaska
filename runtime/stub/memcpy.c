#include <string.h>
#include <stdint.h>
#include <endian.h>

void *memcpy(void *restrict dest, const void *restrict src, size_t n) {
  unsigned char *d = dest;
  const unsigned char *s = src;
	for (size_t i = 0; i < n; i++) {
		d[i] = s[i];
	}
  return dest;

}

