#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <alaska.h>

void square(int *x) {
	*x *= *x;
}


int main() {
	int *x = halloc(sizeof(*x));

	*x = 5;
	square(x);

	printf("%d\n", *x);

	hfree(x);
	return 0;

}
