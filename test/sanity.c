#include <stdio.h>
#include <alaska.h>

int main() {
  volatile int *x = (int *)alaska_halloc(sizeof(int));

	x = (int *)alaska_hrealloc((void*)x, sizeof(int) * 400);
  *x = 42;
  printf("It works! x=0x%zx\n", (uintptr_t)x);
	alaska_hfree((void*)x);
  return 0;
}
