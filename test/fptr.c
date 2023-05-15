#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void *fake_memcpy(void *d, const void *s, unsigned long n) {
  return NULL;
}

void *(*do_memcpy)(void *, const void *, unsigned long);


int main() {
  char *buf1 = malloc(512);
  char *buf2 = malloc(512);

  do_memcpy = memcpy;
  if (rand()) {
    do_memcpy = fake_memcpy;
  }


  do_memcpy(buf1, buf2, 512);

	free(buf1);
	free(buf2);

  return 0;
}
