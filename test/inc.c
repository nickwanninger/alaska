#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <alaska.h>

// #pragma alaska
void inc(volatile int *x) {
	(*x) += 1;
	// alaska_barrier();
}

// #pragma alaska
int main(int argc, char **argv) {
  // volatile int *ptr = (volatile int *)malloc(sizeof(int));
  // *ptr = 0;
  // printf("%d\n", *ptr);
  // inc(ptr);
  // printf("%d\n", *ptr);
  //
  // free((void*)ptr);
  return 0;
}
