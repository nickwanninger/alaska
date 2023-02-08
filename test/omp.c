#include <stdio.h>
#include <stdlib.h>

int sum(int *array, int len) {
  int sum = 0;
#pragma omp parallel for reduction(+ : sum)
  for (int i = 0; i < len; i++) {
    sum += array[i];
  }
  return sum;
}

int main(int argc, char **argv) {
	int len = atoi(argv[1]);
	int *x = calloc(sizeof(int), len);

#pragma omp parallel for
	for (int i = 0; i < len; i++) {
		x[i] = i;
	}

	printf("sum=%d\n", sum(x, len));
}
