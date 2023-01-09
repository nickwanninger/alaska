#include <alaska.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>



void inc(int *x) {
	(*x) += 1;
	if (rand()) {
		(*x) *= 2;
	}
}

int main(int argc, char **argv) {
  int *ptr = (int *)malloc(sizeof(int));
  *ptr = 0;
  printf("%d\n", *ptr);
  inc(ptr);
  printf("%d\n", *ptr);

  free(ptr);
  return 0;
}
