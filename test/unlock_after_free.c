#include <stdlib.h>
#include <stdio.h>

int main(void) {

	int *x = malloc(sizeof(int));


	printf("%d, %p\n", *x, x);

	if (rand()) {
		free(x);
	}
	return 0;
}
