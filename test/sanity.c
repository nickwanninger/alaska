#include <stdio.h>
#include <alaska.h>

int main() {
  volatile int *x = (int *)halloc(sizeof(int));

	x = (int *)hrealloc((void*)x, sizeof(int) * 400);
  *x = 42;
  printf("It works! x=0x%zx\n", (uintptr_t)x);
	hfree((void*)x);
  return 0;
}
