#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void square(int *x) {
	*x *= *x;
}


int main() {
	int *x = malloc(sizeof(*x));


	*x = 5;
	square(x);

	printf("%d\n", *x);

	free(x);
	return 0;

}
