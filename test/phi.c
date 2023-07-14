#include <stdlib.h>

int foo(int num, int *b, int c) {
	if (c == 1) {
		b = NULL;
	}
	for (int i = 0; i < num; i++) {
		*b = rand();
		b++;
	}
	return 0;
}

int main() {
	return 0;
}
