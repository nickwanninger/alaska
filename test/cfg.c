#include <stdlib.h>
#include <stdio.h>

__attribute__((noinline)) int *source(void) { return malloc(sizeof(int)); }

int *test() {
  int *a = source();
  int *b = source();
  int *c = source();
  if (rand()) {
		if (rand()) {
			*a *= 10;
		} else {
			*b += 10;
		}
  } else {
		if (rand()) {
			*c -= 10;
		}
		*c += 40;
  }
	if (rand()) return a;
	if (rand()) return b;
	if (rand()) return c;
  return a;
}

int main() { return 0; }
