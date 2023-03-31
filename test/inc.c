#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <alaska.h>

__attribute__((noinline)) void inc(volatile int *x) {
  (*x) += 1;
}

// #pragma alaska
int main(int argc, char **argv) {
  // for (int i = 0; i < 16; i++) {
  volatile int *ptr = (volatile int *)malloc(sizeof(int));
  *ptr = 0;

  printf("%d\n", *ptr);

  anchorage_manufacture_locality((void *)ptr);
  // inc(ptr);
  printf("%d\n", *ptr);
  // }

  return 0;
}


#if 0



///////////////// INPUT PROGRAM /////////////////
void inc(int *x) {
  (*x) += 1;
}

int main(int argc, char **argv) {
  volatile int *ptr = (volatile int *)malloc(sizeof(int));
	*ptr = 0;
  for (int i = 0; i < 16; i++) {
    inc(ptr);
  }
  printf("%d\n", *ptr);
  return 0;
}

///////////////// NAIVE OUTPUT /////////////////
void inc(int *x) {
	int *y = lock(x);
  (*y) += 1;
	unlock(x);
}

int main(int argc, char **argv) {
  int *ptr = (volatile int *)malloc(sizeof(int));
	int *ptr_locked = lock(ptr);
	*ptr_locked = 0;
  for (int i = 0; i < 16; i++) {
    inc(ptr);
  }
  printf("%d\n", *ptr_locked);
	unlock(ptr);
  return 0;
}

///////////////// WHAT I WANT /////////////////
void inc(int *x, int *y) {
  (*y) += 1;
}

int main(int argc, char **argv) {
  int *ptr = (volatile int *)malloc(sizeof(int));
	int *ptr_locked = lock(ptr);
	*ptr_locked = 0;
  for (int i = 0; i < 16; i++) {
    inc(ptr, ptr_locked);
  }
  printf("%d\n", *ptr_locked);
	unlock(ptr);
  return 0;
}
#endif
