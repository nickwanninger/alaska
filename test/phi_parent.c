#include <stdlib.h>
#include <stdlib.h>
#include <stdio.h>

int main() {
	return 0;
}

int foo(int *a, int *b) {
	int *x = NULL;
	if (rand()) {
		*a += 1;
		x = a;
		printf("a: %p\n", a);
	} else {
		x = b;
		printf("b: %p\n", b);
	}
	return *x;
}
