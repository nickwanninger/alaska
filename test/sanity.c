#include <stdio.h>
#include <alaska.h>

int main() {
  int *x = (int *)halloc(sizeof(int));
  *x = 42;
  printf("It works!\n");
  return 0;
}