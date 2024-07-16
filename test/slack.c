#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

volatile void *ptr = NULL;

int main() {
  for (size_t sz = 0; sz < 1024; sz++) {
    ptr = malloc(sz);
  }
  return 0;
}
