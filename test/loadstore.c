
#include <stdlib.h>
__attribute__((noinline)) int *source(void) { return malloc(sizeof(int)); }


int *test() {
  int *x = source();
  *x += 12;
	if (rand()) *x *= 4;
  return x;
}

int main() { return 0; }
