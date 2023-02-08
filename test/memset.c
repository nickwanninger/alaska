#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define SIZE (4096 * 16)

int main() {
  volatile char* buf = malloc(SIZE);
  buf[0] = 'a';

  memset((void*)buf, 0, SIZE);
  printf("buf = %p\n", buf);

  free((void*)buf);
  return 0;
}
