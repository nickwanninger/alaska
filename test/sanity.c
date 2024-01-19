#include <stdio.h>
#include <alaska.h>

#include <stdint.h>


int main() {
  volatile int *x = (int *)halloc(sizeof(int));

	x = (int *)hrealloc((void*)x, sizeof(int) * 400);
  *x = 42;
  printf("It works! x=%zx\n", (uint64_t)x);
	hfree((void*)x);
  return 0;
}
